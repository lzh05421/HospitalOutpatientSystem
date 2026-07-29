# 医院门诊系统高并发/分布式重构分阶段执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保留现有 C++/Qt 单体服务稳定性的前提下，引入 Redis、MQ、Outbox 和 WebSocket，把挂号、处方通知、库存等热点链路升级为可幂等、可恢复、可观测的企业级架构。

**Architecture:** 第一阶段不做大拆服务，先在当前 server 内部建立分布式可靠性基础设施：配置、幂等表、Outbox 表、Redis/Lua 封装、MQ 消费器、WebSocket 网关。挂号仍以 MySQL 为最终事实源，Redis 负责高并发入口限流和预扣减；处方通知通过 Outbox 保证“业务提交成功则事件必然可发布”。

**Tech Stack:** C++17, Qt Sql/Network/WebSockets, MySQL/InnoDB, Redis, Lua, RabbitMQ 或 Redis Streams, JSON, Outbox Pattern, Idempotency, Reconciliation Jobs.

---

## 一、关键风险点头脑风暴

### P0 数据一致性风险

当前 `RegistrationService.cpp` 已经用 MySQL 事务和条件更新防止号源超卖：

```sql
UPDATE doctor_schedules
SET remain_quota = remain_quota - 1
WHERE id = :id AND remain_quota > 0;
```

引入 Redis 后，风险会变成“Redis 扣成功，但 MySQL 写挂号失败”。因此不能把 Redis 当最终库存，只能当高并发闸门；MySQL 仍是最终事实源。解决策略是：`request_id` 幂等、MQ 重试、MySQL 条件扣减二次兜底、失败补偿 Redis、定时对账。

### P0 重复请求和重复消费风险

Qt 客户端、网络重试、MQ redelivery 都可能导致同一个挂号或处方事件重复处理。解决策略是所有写命令带 `request_id`，数据库用唯一约束阻止重复业务写入，MQ 消费器先插入 `idempotency_records` 再执行业务事务。

### P0 权限和数据范围泄漏风险

分布式以后不能信任客户端传入的角色、医生、科室字段。当前 `RequestRouter.cpp` 会注入 `__operatorUserId`、`__dataScopeCode`、`__departmentScopeIds`、`__doctorId`，后续 Redis/MQ 命令也必须由服务端根据 session 生成上下文，不能直接转发客户端 payload。

### P1 消息丢失风险

处方创建后如果直接 WebSocket 推送，服务进程崩溃、药房客户端离线都会丢通知。解决策略是 Outbox：在创建/审核处方的同一个 MySQL 事务内写 `outbox_events`，后台发布器异步投递 MQ/WebSocket，失败可重试。

### P1 缓存穿透和缓存不一致风险

排班变更、退号、批量重排都会修改 `doctor_schedules.remain_quota`。Redis 号源 key 必须有版本号或同步时间；重排后旧 key 不能继续被扣。解决策略是 `quota_version` 字段、Redis key 携带版本，排班变更后刷新 key 并使旧版本自然过期。

### P1 事务边界膨胀风险

如果在 MySQL 事务内调用 Redis/MQ/WebSocket，会导致外部系统故障拖垮核心事务。解决策略是事务内只写本地表：业务表、幂等表、Outbox 表；事务外由 worker 发布消息和推送通知。

### P2 运维复杂度风险

Redis、MQ、WebSocket 增加了可用性和排障成本。解决策略是尽早加 `system/health`、Outbox 积压数、MQ 死信数、Redis/MySQL 差异数、重试次数和最后错误。

---

## 二、迁移路线选择

### 方案 A：先模块化单体，再分布式化热点链路（推荐）

保留当前 Qt client/server 协议和模块结构，先新增基础设施组件，再逐步替换挂号、处方通知、库存扣减。优点是风险最低，适合毕业设计/企业级展示；缺点是短期内仍是一个主服务进程。

### 方案 B：直接拆微服务

把挂号、处方、库存、通知直接拆成独立服务。优点是架构名词漂亮；缺点是当前权限、事务、部署、日志、测试都会同时复杂化，容易出现“服务拆了，可靠性没上来”。

### 方案 C：只加 Redis 缓存，不加 MQ/Outbox

开发最快，但只能提升查询或入口扣减性能，无法证明消息可靠性，也解决不了处方通知不丢失的问题。

**建议采用方案 A。** 等 Phase 1-4 稳定后，再把 `registration-service`、`notification-service`、`inventory-service` 从单体中拆出，Qt 客户端仍只访问统一网关。

---

## 三、数据库表结构变更

### 1. 排班号源表增强

修改 `doctor_schedules`，支持 Redis 版本化缓存和对账：

```sql
ALTER TABLE doctor_schedules
  ADD COLUMN quota_version BIGINT NOT NULL DEFAULT 0 AFTER remain_quota,
  ADD COLUMN redis_quota_key VARCHAR(128) NULL AFTER quota_version,
  ADD COLUMN redis_synced_at DATETIME NULL AFTER redis_quota_key,
  ADD COLUMN updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP AFTER status,
  ADD INDEX idx_schedule_quota_sync (status, redis_synced_at),
  ADD INDEX idx_schedule_quota_version (id, quota_version);
```

使用规则：

- 每次排班新增、修改总号源、停诊、批量重排时 `quota_version = quota_version + 1`。
- Redis key 使用 `schedule:{schedule_id}:v{quota_version}:remain`。
- `redis_synced_at` 记录最近一次把 MySQL 号源同步到 Redis 的时间。

### 2. 挂号表增强

修改 `registrations`，支持异步命令幂等和状态追踪：

```sql
ALTER TABLE registrations
  ADD COLUMN request_id VARCHAR(64) NULL AFTER id,
  ADD COLUMN source_channel VARCHAR(32) NOT NULL DEFAULT 'QT_CLIENT' AFTER request_id,
  ADD COLUMN confirmed_at DATETIME NULL AFTER register_time,
  ADD COLUMN fail_reason VARCHAR(500) NULL AFTER confirmed_at,
  ADD UNIQUE KEY uk_registrations_request_id (request_id);
```

说明：

- `request_id` 由服务端生成或接收客户端幂等键后校验，必须全链路传递。
- 同步模式可以在事务提交后立即写 `confirmed_at`。
- 异步 MQ 模式下，前端可先拿到“已受理”，再轮询或 WebSocket 接收最终结果。

### 3. 幂等记录表

新增 `idempotency_records`，所有 MQ command consumer 先写这张表：

```sql
CREATE TABLE IF NOT EXISTS idempotency_records (
    request_id VARCHAR(64) PRIMARY KEY,
    module VARCHAR(64) NOT NULL,
    action VARCHAR(64) NOT NULL,
    business_key VARCHAR(128),
    status VARCHAR(16) NOT NULL DEFAULT 'PROCESSING',
    result_code VARCHAR(32),
    result_message VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_idempotency_module_time (module, action, created_at),
    INDEX idx_idempotency_status_time (status, updated_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

状态建议：`PROCESSING`、`SUCCESS`、`FAILED`、`COMPENSATED`。

### 4. Outbox 事件表

新增 `outbox_events`，处方通知、挂号结果、库存告警都可复用：

```sql
CREATE TABLE IF NOT EXISTS outbox_events (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    event_id VARCHAR(64) NOT NULL UNIQUE,
    event_type VARCHAR(64) NOT NULL,
    aggregate_type VARCHAR(64) NOT NULL,
    aggregate_id BIGINT NOT NULL,
    business_key VARCHAR(128),
    payload JSON NOT NULL,
    headers JSON NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'NEW',
    retry_count INT NOT NULL DEFAULT 0,
    next_retry_at DATETIME NULL,
    locked_by VARCHAR(64) NULL,
    locked_at DATETIME NULL,
    last_error VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    published_at DATETIME NULL,
    INDEX idx_outbox_poll (status, next_retry_at, created_at),
    INDEX idx_outbox_aggregate (aggregate_type, aggregate_id),
    INDEX idx_outbox_business_key (business_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

状态建议：`NEW`、`PUBLISHING`、`PUBLISHED`、`FAILED`、`DEAD`。

### 5. 处方表增强

修改 `prescriptions`，支持版本、状态更新时间和通知追踪：

```sql
ALTER TABLE prescriptions
  ADD COLUMN version BIGINT NOT NULL DEFAULT 0 AFTER status,
  ADD COLUMN reviewed_at DATETIME NULL AFTER created_at,
  ADD COLUMN dispensed_at DATETIME NULL AFTER reviewed_at,
  ADD COLUMN updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP AFTER dispensed_at,
  ADD INDEX idx_prescription_status_updated (status, updated_at);
```

说明：

- Outbox 是可靠通知核心，不建议只靠 `notification_status` 字段。
- `version` 用于乐观锁，避免审核/发药并发更新覆盖。

### 6. Outbox 消费记录表

如果事件未来同时进入 MQ 和 WebSocket，建议新增消费者幂等表：

```sql
CREATE TABLE IF NOT EXISTS outbox_event_consumptions (
    event_id VARCHAR(64) NOT NULL,
    consumer_name VARCHAR(64) NOT NULL,
    consumed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (event_id, consumer_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 四、挂号模块 Redis Lua 设计

### 1. Redis Key 设计

```text
schedule:{schedule_id}:v{quota_version}:remain
registration:req:{request_id}
registration:req:{request_id}:result
```

TTL 建议：

- 号源 key：到就诊日结束后 24 小时。
- 请求幂等 key：10-30 分钟，覆盖客户端重试窗口。
- 结果 key：10-30 分钟，便于前端查最终状态。

### 2. Lua 脚本职责

Lua 只做四件事：

1. 判断 `request_id` 是否重复。
2. 判断 Redis 号源 key 是否存在。
3. 判断剩余号源是否大于 0。
4. 原子扣减并写请求幂等标记。

```lua
local quotaKey = KEYS[1]
local requestKey = KEYS[2]
local requestTtl = tonumber(ARGV[1])

if redis.call('EXISTS', requestKey) == 1 then
    return 2
end

local remain = tonumber(redis.call('GET', quotaKey))
if remain == nil then
    return -1
end

if remain <= 0 then
    return 0
end

redis.call('DECR', quotaKey)
redis.call('SET', requestKey, 'DEDUCTED', 'EX', requestTtl)
return 1
```

返回值约定：

```text
1  扣减成功，可以投递 MQ
0  号源已满
-1 Redis 号源未预热，降级走 MySQL 条件扣减或触发预热后重试
2  重复请求，返回上次处理结果或“请求处理中”
```

### 3. 挂号请求链路

```text
registration/create
  -> 服务端鉴权和生成 request_id
  -> 查询 schedule_id、quota_version、doctor_id、fee
  -> 执行 Lua 原子预扣 Redis
  -> Lua 返回 1 后发送 RegistrationCreateCommand 到 MQ
  -> 立即返回“挂号请求已受理”
  -> MQ consumer 写 MySQL registrations + bills
  -> 写 outbox_events: RegistrationCreated 或 RegistrationFailed
  -> WebSocket/轮询通知前端最终状态
```

### 4. MySQL 二次兜底

MQ consumer 仍必须保留条件扣减：

```sql
UPDATE doctor_schedules
SET remain_quota = remain_quota - 1
WHERE id = :schedule_id
  AND quota_version = :quota_version
  AND remain_quota > 0
  AND status = 1;
```

如果影响行数为 0：

- MySQL 认为号源不可扣，事务回滚。
- Redis 执行补偿：`INCR schedule:{schedule_id}:v{quota_version}:remain`。
- `idempotency_records.status = 'COMPENSATED'`。
- 写 `RegistrationFailed` Outbox 事件，提示用户刷新号源。

---

## 五、处方通知 Outbox 模式实现步骤

### 1. 处方创建事务内写事件

在 `PrescriptionService::create` 的同一个 MySQL 事务里完成：

```text
INSERT prescriptions
INSERT prescription_items
UPDATE bills
INSERT outbox_events(PrescriptionCreated)
COMMIT
```

事件 payload 示例：

```json
{
  "eventType": "PrescriptionCreated",
  "prescriptionNo": "RX202606090001",
  "registrationNo": "R202606090001",
  "patientName": "张三",
  "doctorId": 12,
  "status": "CREATED",
  "targetChannel": "pharmacy.prescription.pending"
}
```

关键点：如果处方事务回滚，Outbox 事件也回滚；如果事务提交，事件必然存在。这样保证消息不丢失。

### 2. 处方审核事务内写事件

审核从 `CREATED` 改为 `REVIEWED` 时：

```sql
UPDATE prescriptions
SET status = 'REVIEWED',
    version = version + 1,
    reviewed_at = NOW()
WHERE prescription_no = :prescription_no
  AND status = 'CREATED';
```

同事务写：

```text
PrescriptionReviewed -> targetChannel: pharmacy.prescription.dispense
```

### 3. Outbox 发布器

后台 worker 循环执行：

```sql
UPDATE outbox_events
SET status = 'PUBLISHING',
    locked_by = :worker_id,
    locked_at = NOW()
WHERE status IN ('NEW', 'FAILED')
  AND (next_retry_at IS NULL OR next_retry_at <= NOW())
ORDER BY id
LIMIT 50;
```

然后查询自己锁住的事件，发布到 MQ 或直接交给 WebSocket gateway。发布成功：

```sql
UPDATE outbox_events
SET status = 'PUBLISHED',
    published_at = NOW(),
    last_error = NULL
WHERE id = :id;
```

发布失败：

```sql
UPDATE outbox_events
SET status = CASE WHEN retry_count >= 10 THEN 'DEAD' ELSE 'FAILED' END,
    retry_count = retry_count + 1,
    next_retry_at = DATE_ADD(NOW(), INTERVAL POW(2, LEAST(retry_count, 6)) SECOND),
    last_error = :error
WHERE id = :id;
```

### 4. WebSocket 只负责实时性

药房客户端连接：

```text
ws://server:8900/notifications?token=<session_token>
```

服务端根据 token 订阅频道：

```text
pharmacy.prescription.pending
pharmacy.prescription.dispense
registration.result.{operator_user_id}
```

WebSocket 断开不会丢业务数据，因为药房页面仍可通过 `prescription/list` 查询 MySQL 权威状态。WebSocket 只是让页面及时刷新。

---

## 六、分阶段执行计划

### Phase 0：基线冻结与可回滚迁移

**优先级：P0**  
**难点：中**  
**目标：** 在不改变业务行为的前提下，为后续分布式改造建立基线。

- [ ] 跑通现有 `auth_router_tests`、`schedule_*_tests`、`workflow_rules_tests`。
- [ ] 整理当前挂号、退号、处方创建、审核、发药的状态流。
- [ ] 新增 SQL migration 文件，不直接只改 `schema.sql`；`DatabaseManager::ensureCompatibilitySchema()` 保持兼容自升级。
- [ ] 给关键写接口统一生成或透传 `request_id`，先只记录不改变执行路径。

验收标准：

- 现有功能无行为变化。
- 新字段可重复执行 migration。
- 每个写请求日志里能看到 `request_id`。

### Phase 1：可靠性基础表与基础设施封装

**优先级：P0**  
**难点：中**  
**目标：** 建好幂等、Outbox、基础 Redis/MQ/WebSocket 配置，但不急着替换主链路。

- [ ] 新增 `idempotency_records`、`outbox_events`、`outbox_event_consumptions`。
- [ ] 增强 `doctor_schedules`、`registrations`、`prescriptions` 字段。
- [ ] 新增 `RedisClient`，只封装 `GET/SET/EVALSHA/INCR/DECR`。
- [ ] 新增 `MessageQueue` 接口，先支持 RabbitMQ 或 Redis Streams 二选一。
- [ ] 新增 `OutboxRepository`，提供 `insertEvent()`、`lockBatch()`、`markPublished()`、`markFailed()`。

验收标准：

- 不启用 Redis/MQ 时系统仍能走当前 MySQL 同步流程。
- 启用配置后能连接 Redis/MQ，并在健康检查看到状态。

### Phase 2：挂号 Redis Lua 原子扣减

**优先级：P0**  
**难点：高**  
**目标：** 把挂号高并发入口从“所有请求打 MySQL”改为“Redis Lua 先原子预扣，MySQL 最终确认”。

- [ ] 排班生成/修改后同步 Redis：`schedule:{id}:v{quota_version}:remain`。
- [ ] 实现 `registration_quota_deduct.lua`。
- [ ] `registration/create` 新增灰度开关：`registration.redisQuota.enabled`。
- [ ] Lua 成功后投递 `RegistrationCreateCommand` 到 MQ。
- [ ] Lua 返回 `-1` 时降级到当前 MySQL 条件扣减，避免 Redis 冷启动影响挂号。

验收标准：

- 100 个并发请求抢 10 个号，最终成功挂号数等于 10。
- Redis 剩余数、MySQL `remain_quota`、`registrations` 数量一致。
- 重复 `request_id` 不会创建重复挂号和账单。

### Phase 3：挂号 MQ Consumer 与补偿闭环

**优先级：P0**  
**难点：高**  
**目标：** 保证 Redis 扣成功后，MySQL 持久化最终可达；失败时可补偿、可追踪。

- [ ] 实现 `RegistrationCommandConsumer`。
- [ ] 消费事务顺序：插入幂等记录、MySQL 条件扣号、插入挂号、插入账单、更新幂等状态、写 Outbox。
- [ ] 消费成功后 ACK。
- [ ] 业务失败写 `RegistrationFailed` Outbox。
- [ ] 达到最大重试后进入死信，并执行 Redis 补偿。

验收标准：

- 模拟 MQ 重复投递，只产生一条 `registrations` 和一条 `bills`。
- 模拟 MySQL 失败后恢复，消息重试能成功落库。
- 模拟排班版本失效，Redis 预扣能被补偿。

### Phase 4：处方 Outbox 与 WebSocket 通知

**优先级：P1**  
**难点：中高**  
**目标：** 处方创建/审核后，药房实时收到通知，同时保证通知不丢。

- [ ] 在 `PrescriptionService::create` 事务内插入 `PrescriptionCreated` Outbox。
- [ ] 在 `PrescriptionService::review` 事务内插入 `PrescriptionReviewed` Outbox。
- [ ] 实现 `OutboxPublisher`，支持锁批次、发布、失败退避、死信。
- [ ] 实现 `NotificationGateway`，使用 Qt WebSockets。
- [ ] 客户端药房页面收到通知后刷新 `prescription/list`。

验收标准：

- 强制 WebSocket 断开时，Outbox 事件不丢，药房重新登录仍能看到待处理处方。
- 强制发布器崩溃后重启，`NEW/FAILED` 事件可继续发布。
- 同一事件重复发布时，客户端刷新幂等，不造成重复业务写。

### Phase 5：库存 Redis/MQ 改造

**优先级：P1**  
**难点：高**  
**目标：** 复用挂号经验，把处方发药库存扣减改造成 Redis Lua 多药品原子预扣 + MySQL 最终落库。

- [ ] 药品库存 key：`drug:{drug_id}:stock`。
- [ ] 发药请求 key：`inventory:req:{prescription_no}`。
- [ ] Lua 先检查所有药品库存都充足，再一次性 `DECRBY`。
- [ ] MQ consumer 写 `drugs.stock_quantity`、`stock_records`、`prescriptions.status = DISPENSED`。
- [ ] MySQL `FOR UPDATE` 保留为最终兜底。

验收标准：

- 多药品处方任一库存不足时，不扣任何药品。
- 重复发药请求不会重复扣库存。
- Redis/MySQL 库存差异可被对账任务发现和修正。

### Phase 6：观测、对账与拆服务准备

**优先级：P2**  
**难点：中**  
**目标：** 让系统从“能跑”变成“可运维、可演示、可恢复”。

- [ ] 新增 `system/health`：MySQL、Redis、MQ、WebSocket、Outbox 积压、死信数量。
- [ ] 新增对账任务：号源对账、库存对账、Outbox 卡住事件重置、幂等记录清理。
- [ ] Dashboard 展示 Redis/MQ/Outbox 状态。
- [ ] 编写压测脚本和故障注入脚本。
- [ ] 稳定后再拆服务：`registration-service`、`notification-service`、`inventory-service`。

验收标准：

- 能演示 Redis 宕机降级、MQ 重启重试、WebSocket 离线不丢通知。
- 能给出压测结果：并发挂号成功数、失败数、平均响应时间、最终一致性延迟。

---

## 七、最终优先级排序

| 优先级 | 阶段 | 核心收益 | 难点 |
|---|---|---|---|
| P0 | Phase 0 基线冻结 | 避免重构时破坏已跑通 CRUD/RBAC | 中 |
| P0 | Phase 1 基础设施表 | 幂等、Outbox、配置、健康检查基础 | 中 |
| P0 | Phase 2 Redis Lua 挂号 | 高并发防超卖，企业级亮点最强 | 高 |
| P0 | Phase 3 MQ Consumer 补偿 | 保证 Redis/MySQL 最终一致 | 高 |
| P1 | Phase 4 处方 Outbox/WebSocket | 通知可靠不丢，药房实时协作 | 中高 |
| P1 | Phase 5 库存 Redis/MQ | 发药库存高并发防超卖 | 高 |
| P2 | Phase 6 观测和拆服务 | 可运维、可演示、可继续扩展 | 中 |

---

## 八、实施原则

1. MySQL 是最终事实源，Redis 只做高并发入口和缓存。
2. 所有写操作必须有 `request_id`，所有 consumer 必须幂等。
3. 业务事务内只写 MySQL，不调用 WebSocket，不等待 MQ 发布结果。
4. Outbox 事件和业务数据必须在同一个事务提交。
5. WebSocket 只承诺实时提醒，不承诺业务状态保存。
6. 每次引入分布式组件都要配降级、重试、补偿和对账。
7. 不先拆微服务；先把当前单体做成“模块化、事件化、可恢复”的企业级单体。
