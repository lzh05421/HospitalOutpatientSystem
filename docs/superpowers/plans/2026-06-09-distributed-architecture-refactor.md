# Hospital Distributed Architecture Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade the current C++/Qt hospital outpatient monolith into an enterprise-ready architecture with RBAC data isolation, Redis-based high-concurrency registration, MQ-backed consistency, and WebSocket prescription notifications.

**Architecture:** Keep the current C++/Qt client and server as the main application during the first phases, then introduce infrastructure sidecars/modules incrementally: RBAC authorization middleware, Redis inventory/slot cache, MQ consumers, Outbox event dispatcher, and WebSocket notification gateway. Avoid a big-bang microservice split until permissions, idempotency, and events are stable inside the existing server.

**Tech Stack:** C++17, Qt Network/Sql/WebSockets, MySQL, Redis, Lua scripts, RabbitMQ or Redis Streams, JSON protocol, Outbox Pattern, RBAC tables, service-layer authorization guards.

---

## Brainstorming: Key Risks Moving From Monolith to Distributed Architecture

| Risk | Why It Matters In This Project | Control Strategy |
|---|---|---|
| Distributed consistency | Registration, billing, schedule quota, and inventory updates currently depend on local MySQL transactions. Redis/MQ introduces eventual consistency. | Use idempotency keys, Outbox Pattern, retry tables, compensating Redis stock rollback, and MySQL conditional updates. |
| Over-splitting too early | Splitting 15 modules into services before permissions and events are stable will multiply failure points. | First create modular internal services, then split only hot paths: registration, inventory, notification. |
| Redis/MySQL mismatch | Redis may say quota is deducted while MySQL write fails. | Use MQ consumer retry, reconciliation job, and MySQL as final source of truth. |
| Duplicate messages | MQ redelivery may create duplicate registrations or inventory deductions. | Every write command carries `request_id`; database tables have unique keys for idempotency. |
| Permission leakage | Current role checks are mostly module/action based. Distributed services may forget data-scope filters. | Centralize authorization in `AuthorizationService`; every query receives a `DataScope`. |
| Client trust problem | Qt client can be modified, so role/doctor/department from payload cannot be trusted. | Server derives user identity, role, doctor_id, department scope from session/token only. |
| Realtime message loss | WebSocket clients can disconnect; direct push alone loses events. | Use Outbox table as durable event source; WebSocket only delivers notifications. |
| Operational complexity | Redis, MQ, WebSocket gateway, and server processes need observability. | Add health checks, event status dashboard, retry count, dead-letter queues, and operation logs. |

## Priority Roadmap

1. **Phase 1: RBAC and data-scope foundation**  
   Difficulty: Medium. Highest priority because every later distributed module depends on trusted identity and data isolation.

2. **Phase 2: Registration concurrency with Redis Lua and MQ**  
   Difficulty: High. Most valuable technical highlight, but also highest consistency risk.

3. **Phase 3: Prescription notification with Outbox and WebSocket**  
   Difficulty: Medium-high. Good enterprise collaboration highlight with manageable blast radius.

4. **Phase 4: Inventory Redis/MQ stock deduction**  
   Difficulty: High. Similar to registration, but with more SKU and prescription-state edge cases.

5. **Phase 5: Observability, reconciliation, and service split readiness**  
   Difficulty: Medium. Turns the system from demo architecture into operable architecture.

---

### Task 1: Add RBAC Database Schema

**Files:**
- Modify: `database/schema.sql`
- Modify: `server/src/DatabaseManager.cpp`
- Later code users: `server/src/RequestRouter.cpp`, module services

- [ ] **Step 1: Add RBAC base tables to `database/schema.sql`**

Add these tables after existing `users`, `roles`, and `departments` definitions or as migration-compatible `CREATE TABLE IF NOT EXISTS` statements:

```sql
CREATE TABLE IF NOT EXISTS permissions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    permission_code VARCHAR(128) NOT NULL UNIQUE,
    permission_name VARCHAR(128) NOT NULL,
    module_code VARCHAR(64) NOT NULL,
    action_code VARCHAR(64) NOT NULL,
    description VARCHAR(255),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS role_permissions (
    role_id BIGINT NOT NULL,
    permission_id BIGINT NOT NULL,
    PRIMARY KEY (role_id, permission_id),
    FOREIGN KEY (role_id) REFERENCES roles(id),
    FOREIGN KEY (permission_id) REFERENCES permissions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS user_roles (
    user_id BIGINT NOT NULL,
    role_id BIGINT NOT NULL,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (role_id) REFERENCES roles(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS data_scopes (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    scope_code VARCHAR(32) NOT NULL UNIQUE,
    scope_name VARCHAR(64) NOT NULL,
    description VARCHAR(255)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS role_data_scopes (
    role_id BIGINT NOT NULL,
    scope_id BIGINT NOT NULL,
    PRIMARY KEY (role_id, scope_id),
    FOREIGN KEY (role_id) REFERENCES roles(id),
    FOREIGN KEY (scope_id) REFERENCES data_scopes(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS user_department_scope (
    user_id BIGINT NOT NULL,
    department_id BIGINT NOT NULL,
    PRIMARY KEY (user_id, department_id),
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (department_id) REFERENCES departments(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

- [ ] **Step 2: Seed scope constants**

```sql
INSERT IGNORE INTO data_scopes (scope_code, scope_name, description) VALUES
('ALL', '全院数据', '可访问全院业务数据'),
('DEPARTMENT', '本科室数据', '只能访问授权科室数据'),
('SELF', '本人数据', '只能访问本人相关数据'),
('PHARMACY_PENDING', '待配药数据', '药房只能访问待审核、待发药处方');
```

- [ ] **Step 3: Seed representative permissions**

```sql
INSERT IGNORE INTO permissions (permission_code, permission_name, module_code, action_code, description) VALUES
('registration:create', '创建挂号', 'registration', 'create', '创建患者挂号记录'),
('registration:list', '查看挂号', 'registration', 'list', '查看挂号列表'),
('schedule:rangeList', '查看范围排班', 'schedule', 'rangeList', '智能排班前读取范围内完整排班'),
('consultation:save', '保存接诊', 'consultation', 'save', '医生保存接诊记录'),
('prescription:create', '开立处方', 'prescription', 'create', '医生创建处方'),
('prescription:review', '审核处方', 'prescription', 'review', '药师审核处方'),
('prescription:dispense', '发药', 'prescription', 'dispense', '药师发药并扣减库存'),
('inventory:update', '维护库存', 'inventory', 'update', '维护药品库存');
```

- [ ] **Step 4: Add compatibility creation in `DatabaseManager::ensureCompatibilitySchema()`**

Add the same `CREATE TABLE IF NOT EXISTS` SQL strings in `server/src/DatabaseManager.cpp` so existing databases self-upgrade when the server starts.

- [ ] **Step 5: Verify schema creation**

Run:

```powershell
cmake --build build-windows --target hospital_server
```

Expected: server target builds successfully. Then start server once against a test database and verify:

```sql
SHOW TABLES LIKE 'permissions';
SHOW TABLES LIKE 'user_department_scope';
```

Expected: both tables exist.

---

### Task 2: Introduce AuthorizationService and DataScope

**Files:**
- Create: `server/include/server/AuthorizationService.h`
- Create: `server/src/AuthorizationService.cpp`
- Modify: `server/CMakeLists.txt`
- Modify: `server/src/RequestRouter.cpp`

- [ ] **Step 1: Define authorization result types**

```cpp
struct DataScope
{
    QString scopeCode;
    QVector<qint64> departmentIds;
    qint64 doctorId = 0;
};

struct PermissionDecision
{
    bool allowed = false;
    QString message;
    DataScope dataScope;
};
```

- [ ] **Step 2: Implement permission lookup**

`AuthorizationService::canAccess(userId, roleCode, module, action)` should:

1. Allow `ADMIN` with `ALL` scope.
2. Query `permissions.permission_code = module + ":" + action`.
3. Check `role_permissions`.
4. Resolve data scope from `role_data_scopes` and `user_department_scope`.
5. Return `PermissionDecision`.

- [ ] **Step 3: Inject trusted data scope into request payload**

In `RequestRouter::route()` add:

```cpp
routedRequest.payload["__dataScopeCode"] = decision.dataScope.scopeCode;
routedRequest.payload["__doctorId"] = QString::number(decision.dataScope.doctorId);
QJsonArray departments;
for (qint64 id : decision.dataScope.departmentIds) {
    departments.append(QString::number(id));
}
routedRequest.payload["__departmentScopeIds"] = departments;
```

- [ ] **Step 4: Replace static permission map gradually**

Keep the current hard-coded map as fallback during migration. RBAC table decisions take precedence when table data exists.

- [ ] **Step 5: Verify**

Run:

```powershell
cmake --build build-windows --target auth_router_tests
ctest --test-dir build-windows -R auth_router_tests --output-on-failure
```

Expected: existing auth tests still pass.

---

### Task 3: Apply Data Scope Filters to Business Modules

**Files:**
- Modify: `server/src/modules/RegistrationService.cpp`
- Modify: `server/src/modules/ScheduleService.cpp`
- Modify: `server/src/modules/ConsultationService.cpp`
- Modify: `server/src/modules/PrescriptionService.cpp`
- Modify: `server/src/modules/PatientRecordService.cpp`

- [ ] **Step 1: Add helper for department scope SQL**

Create a local helper in each service or shared SQL utility:

```cpp
QString departmentScopeFilter(const QJsonObject& payload, const QString& departmentColumn)
{
    const QString scope = payload.value("__dataScopeCode").toString();
    if (scope == "ALL" || scope.isEmpty()) {
        return {};
    }
    if (scope == "DEPARTMENT") {
        const QJsonArray ids = payload.value("__departmentScopeIds").toArray();
        QStringList values;
        for (const auto& item : ids) {
            values.append(item.toString());
        }
        return values.isEmpty()
            ? " AND 1 = 0 "
            : QString(" AND %1 IN (%2) ").arg(departmentColumn, values.join(","));
    }
    return {};
}
```

- [ ] **Step 2: Apply doctor self scope**

For doctor-facing modules:

```cpp
if (payload.value("__dataScopeCode").toString() == "SELF") {
    filters.append("doc.id = :operator_doctor_id");
    query.bindValue(":operator_doctor_id", payload.value("__doctorId").toString().toLongLong());
}
```

- [ ] **Step 3: Apply pharmacy pending scope**

In `PrescriptionService::list`, when scope is `PHARMACY_PENDING`, constrain:

```sql
pr.status IN ('CREATED', 'REVIEWED')
```

- [ ] **Step 4: Verify no module trusts frontend role fields**

Search:

```powershell
rg -n "__operatorRoleCode|roleCode|departmentScope" server/src/modules
```

Expected: modules use trusted metadata injected by router, not arbitrary client fields.

---

### Task 4: Add Redis Lua Design for Registration Quota Deduction

**Files:**
- Create: `server/include/server/RedisClient.h`
- Create: `server/src/RedisClient.cpp`
- Create: `server/scripts/registration_quota_deduct.lua`
- Modify: `server/src/modules/RegistrationService.cpp`

- [ ] **Step 1: Add Lua script**

Create `server/scripts/registration_quota_deduct.lua`:

```lua
local quotaKey = KEYS[1]
local requestKey = KEYS[2]
local ttl = tonumber(ARGV[1])

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
redis.call('SET', requestKey, '1', 'EX', ttl)
return 1
```

Return values:

| Value | Meaning |
|---|---|
| `1` | Deducted successfully |
| `0` | Sold out |
| `-1` | Redis quota key missing |
| `2` | Duplicate request |

- [ ] **Step 2: Define Redis keys**

```text
schedule:{schedule_id}:remain
registration:req:{request_id}
```

- [ ] **Step 3: Preload quota on schedule publish/update**

When `schedule/save` or `schedule/batchSave` succeeds:

```text
SET schedule:{schedule_id}:remain remain_quota
```

- [ ] **Step 4: Update `registration/create` flow**

New flow:

```text
validate patient and schedule
generate request_id or read from payload
execute Lua script
if sold out -> return "号源已满"
if success -> publish registration command to MQ
return "挂号请求已受理"
```

- [ ] **Step 5: Keep MySQL conditional update as final safety**

MQ consumer must execute:

```sql
UPDATE doctor_schedules
SET remain_quota = remain_quota - 1
WHERE id = :schedule_id
  AND remain_quota > 0
  AND status = 1;
```

If affected rows is 0, publish compensation:

```text
INCR schedule:{schedule_id}:remain
```

- [ ] **Step 6: Verify**

Write an integration test that sends 100 concurrent requests for a schedule with quota 10. Expected:

```text
10 success or accepted
90 sold out
doctor_schedules.remain_quota = 0
registrations count for schedule = 10
```

---

### Task 5: Add MQ Command Consumer for Registration

**Files:**
- Create: `server/include/server/MessageQueue.h`
- Create: `server/src/MessageQueue.cpp`
- Create: `server/src/workers/RegistrationCommandConsumer.cpp`
- Modify: `server/CMakeLists.txt`

- [ ] **Step 1: Define message payload**

```json
{
  "eventType": "RegistrationCreateCommand",
  "requestId": "REGREQ-20260609-0001",
  "patientId": 1,
  "scheduleId": 10,
  "operatorId": 3,
  "fee": 23.00
}
```

- [ ] **Step 2: Add idempotency table**

```sql
CREATE TABLE IF NOT EXISTS idempotency_records (
    request_id VARCHAR(64) PRIMARY KEY,
    module VARCHAR(64) NOT NULL,
    action VARCHAR(64) NOT NULL,
    business_key VARCHAR(128),
    status VARCHAR(16) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

- [ ] **Step 3: Consumer transaction**

Consumer transaction order:

1. Insert `idempotency_records`.
2. Conditional update `doctor_schedules`.
3. Insert `registrations`.
4. Insert `bills`.
5. Commit.
6. Ack MQ message.

- [ ] **Step 4: Failure handling**

If MySQL transaction fails:

1. Rollback.
2. Nack/retry message.
3. After max retry, move to dead-letter queue.
4. Compensate Redis quota if necessary.

---

### Task 6: Add Outbox Table for Prescription Events

**Files:**
- Modify: `database/schema.sql`
- Modify: `server/src/DatabaseManager.cpp`
- Modify: `server/src/modules/PrescriptionService.cpp`

- [ ] **Step 1: Add Outbox schema**

```sql
CREATE TABLE IF NOT EXISTS outbox_events (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    event_type VARCHAR(64) NOT NULL,
    aggregate_type VARCHAR(64) NOT NULL,
    aggregate_id BIGINT NOT NULL,
    business_key VARCHAR(128),
    payload JSON NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'NEW',
    retry_count INT NOT NULL DEFAULT 0,
    last_error VARCHAR(500),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    published_at DATETIME NULL,
    INDEX idx_outbox_status_created (status, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

- [ ] **Step 2: Write event in same transaction as prescription create**

When `prescription/create` succeeds, in the same MySQL transaction insert:

```json
{
  "eventType": "PrescriptionCreated",
  "prescriptionNo": "RX202606090001",
  "patient": "张三",
  "doctor": "李医生",
  "status": "CREATED"
}
```

- [ ] **Step 3: Write event in same transaction as prescription review**

When `prescription/review` succeeds, insert:

```json
{
  "eventType": "PrescriptionReviewed",
  "prescriptionNo": "RX202606090001",
  "status": "REVIEWED"
}
```

- [ ] **Step 4: Verify transactional behavior**

Force a prescription insert failure after event creation in a test transaction. Expected:

```text
prescription row rollback
outbox event rollback
```

---

### Task 7: Add WebSocket Notification Gateway

**Files:**
- Create: `server/include/server/NotificationGateway.h`
- Create: `server/src/NotificationGateway.cpp`
- Create: `server/src/workers/OutboxPublisher.cpp`
- Modify: `server/src/main.cpp`
- Modify: `client/src/MainWindow.cpp` or create client notification component

- [ ] **Step 1: Start WebSocket server**

Use Qt WebSockets:

```cpp
QWebSocketServer notificationServer("HospitalNotificationGateway", QWebSocketServer::NonSecureMode);
notificationServer.listen(QHostAddress::Any, 8900);
```

- [ ] **Step 2: Authenticate WebSocket client**

Client connects with token:

```text
ws://127.0.0.1:8900/notifications?token=<session_token>
```

Server maps token to user role and allowed channels:

```text
pharmacy.prescription.pending
doctor.consultation.result
registration.queue.update
```

- [ ] **Step 3: Poll and publish Outbox events**

`OutboxPublisher` loop:

```sql
SELECT id, event_type, payload
FROM outbox_events
WHERE status = 'NEW'
ORDER BY id
LIMIT 50;
```

After successful WebSocket publish:

```sql
UPDATE outbox_events
SET status = 'PUBLISHED', published_at = NOW()
WHERE id = :id;
```

- [ ] **Step 4: Client handles prescription notifications**

Qt client receives:

```json
{
  "channel": "pharmacy.prescription.pending",
  "eventType": "PrescriptionCreated",
  "message": "有新的待审核处方"
}
```

Pharmacy page refreshes `prescription/list`.

- [ ] **Step 5: Offline behavior**

If pharmacy client is offline, event remains queryable from MySQL because prescription status is authoritative. WebSocket only improves timeliness.

---

### Task 8: Add Inventory Redis Deduction After Prescription Review/Dispense

**Files:**
- Create: `server/scripts/inventory_stock_deduct.lua`
- Modify: `server/src/modules/PrescriptionService.cpp`
- Modify: `server/src/modules/InventoryService.cpp`

- [ ] **Step 1: Lua script**

```lua
local requestKey = KEYS[1]
local ttl = tonumber(ARGV[1])

if redis.call('EXISTS', requestKey) == 1 then
    return 2
end

for i = 2, #KEYS do
    local stock = tonumber(redis.call('GET', KEYS[i]))
    local required = tonumber(ARGV[i])
    if stock == nil then
        return -1
    end
    if stock < required then
        return 0
    end
end

for i = 2, #KEYS do
    redis.call('DECRBY', KEYS[i], tonumber(ARGV[i]))
end

redis.call('SET', requestKey, '1', 'EX', ttl)
return 1
```

- [ ] **Step 2: Key design**

```text
inventory:req:{prescription_no}
drug:{drug_id}:stock
```

- [ ] **Step 3: Dispense flow**

```text
prescription/dispense
  -> verify prescription status REVIEWED
  -> Redis Lua validates and deducts all drugs atomically
  -> MQ command persists stock deduction records
  -> MySQL updates prescription status DISPENSED
```

- [ ] **Step 4: Reconciliation**

Nightly job compares:

```text
Redis drug stock
MySQL drugs.stock_quantity
inventory transaction logs
```

Differences produce operation log and alert.

---

### Task 9: Observability and Reconciliation

**Files:**
- Create: `server/src/modules/SystemHealthService.cpp`
- Modify: `server/src/RequestRouter.cpp`
- Modify: `client/src/pages/DashboardPage.cpp`

- [ ] **Step 1: Add health checks**

Expose:

```text
system/health
```

Response:

```json
{
  "mysql": "UP",
  "redis": "UP",
  "mq": "UP",
  "websocket": "UP",
  "outboxPending": 3,
  "deadLetters": 0
}
```

- [ ] **Step 2: Add reconciliation jobs**

Jobs:

1. Schedule quota reconciliation.
2. Inventory stock reconciliation.
3. Outbox stuck event retry.
4. Idempotency record cleanup.

- [ ] **Step 3: Add dashboard cards**

Dashboard should show:

```text
Redis 状态
MQ 积压
Outbox 待发送
死信消息
库存差异
```

---

## Final Phasing Summary

| Phase | Priority | Main Modules | New Tech | Difficulty | Result |
|---|---:|---|---|---|---|
| 1. RBAC + Data Scope | P0 | auth, all modules | RBAC tables, AuthorizationService | Medium | Enterprise permission foundation |
| 2. Registration High Concurrency | P0 | registration, schedule, billing | Redis, Lua, MQ, idempotency | High | Expert slot anti-oversell |
| 3. Prescription Realtime Notification | P1 | consultation, prescription | Outbox, WebSocket | Medium-high | Doctor-to-pharmacy realtime flow |
| 4. Inventory Distributed Deduction | P1 | inventory, prescription | Redis Lua, MQ, reconciliation | High | Anti-oversell drug stock center |
| 5. Observability | P2 | dashboard, operationLog | Health check, retry, dead-letter, reconciliation | Medium | Operable distributed system |

## Implementation Rule

Do not split into independent microservices before Phase 1 and Phase 2 are stable. First make the current server internally modular and event-driven. After that, split these candidates:

1. `registration-service`
2. `inventory-service`
3. `notification-service`
4. `auth-service`

The Qt client should not know whether the backend is monolith or microservices. It should continue calling stable module/action APIs or a gateway.
