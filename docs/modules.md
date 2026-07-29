# 模块框架说明

## 功能模块

1. 登录权限模块：用户登录、角色权限、会话 token。
2. 患者管理模块：患者档案新增、查询、修改。
3. 挂号管理模块：科室选择、医生号源、挂号、退号。
4. 医生排班模块：医生班次、号源数量、出诊状态。
5. 医生接诊模块：候诊队列、诊断记录、病历摘要。
6. 处方管理模块：开立处方、处方明细、处方审核。
7. 药品库存模块：药品信息、入库、出库、库存预警。
8. 收费结算模块：挂号费、药品费、检查费、支付记录。
9. 费用统计模块：日/月收入、科室收入、药品销售统计。
10. 系统管理模块：科室、医生、员工、字典数据维护。

## 客户端页面规划

- `LoginDialog`：登录。
- `MainWindow`：主窗口和模块导航。
- `PatientPage`：患者管理。
- `RegistrationPage`：挂号管理。
- `SchedulePage`：医生排班。
- `ConsultationPage`：医生接诊。
- `PrescriptionPage`：处方管理。
- `DrugInventoryPage`：药品库存。
- `BillingPage`：收费结算。
- `StatisticsPage`：费用统计。

## 服务端服务规划

- `AuthService`
- `PatientService`
- `RegistrationService`
- `ScheduleService`
- `ConsultationService`
- `PrescriptionService`
- `InventoryService`
- `BillingService`
- `StatisticsService`

通信暂定为 TCP + JSON Lines：客户端每次发送一行 JSON 请求，服务端返回一行 JSON 响应。
