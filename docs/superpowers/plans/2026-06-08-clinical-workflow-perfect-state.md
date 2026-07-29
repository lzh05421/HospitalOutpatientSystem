# Clinical Workflow Perfect State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the outpatient workflow internally consistent and harder to misuse: registration enters the waiting queue, only called or post-exam patients enter doctor consultation, examination completion returns patients to follow-up, prescriptions cannot be dispensed before payment, and tests protect the status rules.

**Architecture:** Add a small workflow rules unit that names registration statuses, page visibility rules, and allowed transitions. Services will call those rules before mutating status so generic UI edits cannot bypass the process. Client pages will reflect the same flow by hiding invalid actions and by waiting for successful consultation save before sending examination or prescription requests.

**Tech Stack:** C++17, Qt Core/Widgets/Test, CMake, CTest, existing TCP JSON request/response protocol, existing MySQL schema with `registrations.status` as `VARCHAR`.

---

## Target Workflow

The optimized workflow is:

```text
挂号成功 WAITING
  -> 候诊队列显示
  -> 叫号 CALLED
  -> 医生接诊显示
  -> 开始接诊 IN_CONSULTATION
  -> 保存完成 FINISHED
     或申请检查 CHECKING
  -> 检查完成 CHECK_DONE
  -> 医生复诊显示
  -> 保存完成 FINISHED
  -> 处方 CREATED
  -> 收费 PAID
  -> 审核 REVIEWED
  -> 发药 DISPENSED
```

This deliberately changes the current duplicated behavior:

- `WAITING` appears in `候诊队列`, not in `医生接诊`.
- `CALLED`, `IN_CONSULTATION`, and `CHECK_DONE` appear in `医生接诊`.
- `CHECKING` means the patient is doing or waiting for examination results, so it does not appear as a normal doctor-ready row.
- `CHECK_DONE` means the result is back and the patient is ready for doctor follow-up.

---

## File Structure

- Create: `server/include/server/WorkflowRules.h`
  - Centralizes status constants, display text, allowed transitions, and page visibility predicates.
- Create: `server/src/WorkflowRules.cpp`
  - Implements pure workflow rules with no database dependency.
- Create: `tests/workflow_rules_tests.cpp`
  - Protects the status machine and page visibility rules.
- Modify: `server/CMakeLists.txt`
  - Adds `WorkflowRules` to `hospital_server_core`.
- Modify: `tests/CMakeLists.txt`
  - Adds `workflow_rules_tests`.
- Modify: `server/src/modules/RegistrationService.cpp`
  - Uses workflow rules for call, update, waiting list, and status display.
- Modify: `server/src/modules/ConsultationService.cpp`
  - Adds `start` action, filters only doctor-ready states, and validates save transitions.
- Modify: `server/src/modules/ExaminationService.cpp`
  - Makes examination completion transition registration from `CHECKING` to `CHECK_DONE`.
- Modify: `server/src/modules/PrescriptionService.cpp`
  - Blocks dispensing until the linked bill is paid.
- Modify: `server/src/modules/BillingService.cpp`
  - Prevents unsafe manual status edits and keeps refund semantics clear.
- Modify: `client/src/pages/WaitingQueuePage.cpp`
  - Keeps call operation as the handoff from waiting to doctor consultation.
- Modify: `client/src/pages/ConsultationPage.cpp`
  - Blocks direct start from `WAITING`, sends `consultation/start`, and chains examination/prescription only after consultation save succeeds.
- Modify: `client/src/pages/PrescriptionPage.cpp`
  - Updates labels to make the payment-before-dispense rule explicit.
- Modify: `client/src/pages/ModulePage.cpp`
  - Disables generic edit/delete for workflow-controlled modules where direct text edits can corrupt state.

---

### Task 1: Add Explicit Workflow Rules

**Files:**
- Create: `server/include/server/WorkflowRules.h`
- Create: `server/src/WorkflowRules.cpp`
- Create: `tests/workflow_rules_tests.cpp`
- Modify: `server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing workflow tests**

Create `tests/workflow_rules_tests.cpp`:

```cpp
#include "server/WorkflowRules.h"

#include <QtTest/QtTest>

using hospital::server::WorkflowRules;

class WorkflowRulesTests : public QObject
{
    Q_OBJECT

private slots:
    void waitingRowsAreVisibleOnlyInWaitingQueue();
    void doctorReadyRowsExcludeRawWaiting();
    void examinationCompletionReturnsPatientForFollowUp();
    void invalidRegistrationStatusIsRejected();
};

void WorkflowRulesTests::waitingRowsAreVisibleOnlyInWaitingQueue()
{
    QVERIFY(WorkflowRules::isWaitingQueueStatus("WAITING"));
    QVERIFY(!WorkflowRules::isDoctorConsultationStatus("WAITING"));
}

void WorkflowRulesTests::doctorReadyRowsExcludeRawWaiting()
{
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("CALLED"));
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("IN_CONSULTATION"));
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("CHECK_DONE"));
    QVERIFY(!WorkflowRules::isDoctorConsultationStatus("CHECKING"));
}

void WorkflowRulesTests::examinationCompletionReturnsPatientForFollowUp()
{
    QCOMPARE(WorkflowRules::statusAfterExaminationCompleted(), QString("CHECK_DONE"));
    QCOMPARE(WorkflowRules::displayText("CHECK_DONE"), QString("检查完成待复诊"));
}

void WorkflowRulesTests::invalidRegistrationStatusIsRejected()
{
    QVERIFY(WorkflowRules::isValidRegistrationStatus("WAITING"));
    QVERIFY(!WorkflowRules::isValidRegistrationStatus("随便填"));
}

QTEST_MAIN(WorkflowRulesTests)
#include "workflow_rules_tests.moc"
```

- [ ] **Step 2: Register test target and verify RED**

Append to `tests/CMakeLists.txt`:

```cmake
add_executable(workflow_rules_tests
    workflow_rules_tests.cpp
)

target_link_libraries(workflow_rules_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME workflow_rules_tests COMMAND workflow_rules_tests)

if(WIN32)
    set_tests_properties(workflow_rules_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>;PATH=path_list_prepend:${TEST_CXX_RUNTIME_DIR}"
    )
endif()
```

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: FAIL because `server/WorkflowRules.h` does not exist.

- [ ] **Step 3: Implement `WorkflowRules`**

Create `server/include/server/WorkflowRules.h`:

```cpp
#pragma once

#include <QString>
#include <QStringList>

namespace hospital::server {

class WorkflowRules
{
public:
    static QString waiting();
    static QString called();
    static QString inConsultation();
    static QString checking();
    static QString checkDone();
    static QString finished();
    static QString cancelled();

    static QStringList validRegistrationStatuses();
    static bool isValidRegistrationStatus(const QString& status);
    static bool isWaitingQueueStatus(const QString& status);
    static bool isDoctorConsultationStatus(const QString& status);
    static bool canCall(const QString& status);
    static bool canStartConsultation(const QString& status);
    static bool canFinishConsultation(const QString& status);
    static bool canCancelRegistration(const QString& status);
    static QString statusAfterExaminationRequested();
    static QString statusAfterExaminationCompleted();
    static QString displayText(const QString& status);
};

} // namespace hospital::server
```

Create `server/src/WorkflowRules.cpp`:

```cpp
#include "server/WorkflowRules.h"

namespace hospital::server {

QString WorkflowRules::waiting() { return QStringLiteral("WAITING"); }
QString WorkflowRules::called() { return QStringLiteral("CALLED"); }
QString WorkflowRules::inConsultation() { return QStringLiteral("IN_CONSULTATION"); }
QString WorkflowRules::checking() { return QStringLiteral("CHECKING"); }
QString WorkflowRules::checkDone() { return QStringLiteral("CHECK_DONE"); }
QString WorkflowRules::finished() { return QStringLiteral("FINISHED"); }
QString WorkflowRules::cancelled() { return QStringLiteral("CANCELLED"); }

QStringList WorkflowRules::validRegistrationStatuses()
{
    return {waiting(), called(), inConsultation(), checking(), checkDone(), finished(), cancelled()};
}

bool WorkflowRules::isValidRegistrationStatus(const QString& status)
{
    return validRegistrationStatuses().contains(status);
}

bool WorkflowRules::isWaitingQueueStatus(const QString& status)
{
    return status == waiting() || status == called() || status == checkDone();
}

bool WorkflowRules::isDoctorConsultationStatus(const QString& status)
{
    return status == called() || status == inConsultation() || status == checkDone();
}

bool WorkflowRules::canCall(const QString& status)
{
    return status == waiting() || status == checkDone();
}

bool WorkflowRules::canStartConsultation(const QString& status)
{
    return status == called() || status == checkDone();
}

bool WorkflowRules::canFinishConsultation(const QString& status)
{
    return status == inConsultation() || status == called() || status == checkDone();
}

bool WorkflowRules::canCancelRegistration(const QString& status)
{
    return status == waiting() || status == called();
}

QString WorkflowRules::statusAfterExaminationRequested()
{
    return checking();
}

QString WorkflowRules::statusAfterExaminationCompleted()
{
    return checkDone();
}

QString WorkflowRules::displayText(const QString& status)
{
    if (status == waiting()) return QStringLiteral("待叫号");
    if (status == called()) return QStringLiteral("已叫号");
    if (status == inConsultation()) return QStringLiteral("接诊中");
    if (status == checking()) return QStringLiteral("检查中");
    if (status == checkDone()) return QStringLiteral("检查完成待复诊");
    if (status == finished()) return QStringLiteral("已接诊");
    if (status == cancelled()) return QStringLiteral("已取消");
    return status;
}

} // namespace hospital::server
```

- [ ] **Step 4: Add source to server core**

In `server/CMakeLists.txt`, add:

```cmake
    include/server/WorkflowRules.h
    src/WorkflowRules.cpp
```

inside `HOSPITAL_SERVER_CORE_SOURCES`.

- [ ] **Step 5: Verify GREEN**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure -R workflow_rules_tests
```

Expected: PASS.

---

### Task 2: Apply Rules to Registration and Waiting Queue

**Files:**
- Modify: `server/src/modules/RegistrationService.cpp`
- Modify: `client/src/pages/WaitingQueuePage.cpp`

- [ ] **Step 1: Include workflow rules**

Add to `server/src/modules/RegistrationService.cpp`:

```cpp
#include "server/WorkflowRules.h"
```

- [ ] **Step 2: Keep new registrations in waiting queue**

Replace the hard-coded insert status:

```cpp
"VALUES (:registration_no, :patient_id, :doctor_id, :schedule_id, :appointment_time_slot, 'WAITING', :fee, 1)");
```

with:

```cpp
"VALUES (:registration_no, :patient_id, :doctor_id, :schedule_id, :appointment_time_slot, :status, :fee, 1)");
query.bindValue(":status", WorkflowRules::waiting());
```

- [ ] **Step 3: Validate manual status updates**

Before `UPDATE registrations SET status = :status`, add:

```cpp
    const QString nextStatus = payload.value("状态").toString().trimmed();
    if (!WorkflowRules::isValidRegistrationStatus(nextStatus)) {
        return {false, "挂号状态不合法，请通过叫号、接诊、检查或取消流程变更状态。", {}};
    }
```

Bind `nextStatus` instead of reading payload again.

- [ ] **Step 4: Restrict cancellation**

In `cancelRegistrationInDatabase()`, replace direct checks for `FINISHED` and `CANCELLED` with:

```cpp
    if (status == WorkflowRules::cancelled()) {
        db.rollback();
        return {true, "该挂号已经是取消状态。", {}};
    }
    if (!WorkflowRules::canCancelRegistration(status)) {
        db.rollback();
        return {false, "当前状态不能退号；已接诊、检查中或检查完成待复诊的记录需走病历/检查流程处理。", {}};
    }
```

- [ ] **Step 5: Restrict call transition**

Replace the call SQL:

```cpp
UPDATE registrations SET status = 'CALLED'
WHERE registration_no = :registration_no AND status IN ('WAITING', 'CHECKING')
```

with:

```cpp
UPDATE registrations SET status = :called
WHERE registration_no = :registration_no AND status IN (:waiting, :check_done)
```

Because Qt SQL does not expand placeholders inside `IN`, use explicit OR:

```cpp
query.prepare("UPDATE registrations SET status = :called "
              "WHERE registration_no = :registration_no "
              "AND (status = :waiting OR status = :check_done)");
query.bindValue(":called", WorkflowRules::called());
query.bindValue(":waiting", WorkflowRules::waiting());
query.bindValue(":check_done", WorkflowRules::checkDone());
```

- [ ] **Step 6: Fix waiting queue filter and display**

Change waiting queue filter from:

```cpp
QStringList filters = {"r.status IN ('WAITING', 'CHECKING', 'CALLED')"};
```

to:

```cpp
QStringList filters = {"r.status IN ('WAITING', 'CALLED', 'CHECK_DONE')"};
```

Update display CASE:

```sql
CASE r.status
WHEN 'WAITING' THEN '待叫号'
WHEN 'CALLED' THEN '已叫号'
WHEN 'CHECK_DONE' THEN '检查完成待复诊'
ELSE r.status END AS '候诊状态'
```

- [ ] **Step 7: Update waiting queue page text**

In `client/src/pages/WaitingQueuePage.cpp`, replace the description with:

```cpp
"挂号成功后进入候诊队列；叫号后转入医生接诊，检查完成后回到这里等待复诊。"
```

- [ ] **Step 8: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 3: Make Doctor Consultation State-Driven

**Files:**
- Modify: `server/src/modules/ConsultationService.cpp`
- Modify: `client/src/pages/ConsultationPage.cpp`

- [ ] **Step 1: Include workflow rules**

Add:

```cpp
#include "server/WorkflowRules.h"
```

to `server/src/modules/ConsultationService.cpp`.

- [ ] **Step 2: Add start action**

Add helper in `ConsultationService.cpp`:

```cpp
common::Response startConsultationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE registrations SET status = :in_consultation "
                  "WHERE registration_no = :registration_no "
                  "AND (status = :called OR status = :check_done)");
    query.bindValue(":in_consultation", WorkflowRules::inConsultation());
    query.bindValue(":registration_no", payload.value("挂号单号").toString().trimmed());
    query.bindValue(":called", WorkflowRules::called());
    query.bindValue(":check_done", WorkflowRules::checkDone());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        return {false, "只有已叫号或检查完成待复诊的患者可以开始接诊。", {}};
    }
    return {true, "已进入接诊中。", {}};
}
```

In `ConsultationService::handle()`, before save handling:

```cpp
    if (request.action == "start") {
        if (m_database->isEnabled()) {
            return startConsultationInDatabase(m_database, request.payload);
        }
        return {true, "已进入接诊中。", {}};
    }
```

- [ ] **Step 3: Validate save transition**

In `saveConsultationInDatabase()`, change the SELECT:

```cpp
query.prepare("SELECT id, doctor_id, status FROM registrations WHERE registration_no = :registration_no LIMIT 1");
```

Read `registrationStatus`, then add:

```cpp
    if (!WorkflowRules::canFinishConsultation(registrationStatus)) {
        db.rollback();
        return {false, "当前状态不能保存接诊，请先叫号或等待检查结果回传。", {}};
    }
```

Bind status using workflow rules:

```cpp
query.bindValue(":status", backToWaiting ? WorkflowRules::statusAfterExaminationRequested() : WorkflowRules::finished());
```

- [ ] **Step 4: Filter doctor consultation rows**

Change consultation list filter from:

```cpp
QStringList filters = {"r.status IN ('WAITING', 'CHECKING', 'CALLED')"};
```

to:

```cpp
QStringList filters = {"r.status IN ('CALLED', 'IN_CONSULTATION', 'CHECK_DONE')"};
```

Update display CASE:

```sql
CASE r.status
WHEN 'CALLED' THEN '已叫号'
WHEN 'IN_CONSULTATION' THEN '接诊中'
WHEN 'CHECK_DONE' THEN '检查完成待复诊'
ELSE r.status END AS '状态'
```

- [ ] **Step 5: Make client start call server-side**

In `ConsultationPage::startConsultation()`, before opening the dialog, reject raw waiting rows:

```cpp
    const QString status = row.value("状态").toString();
    if (status == "待叫号" || status == "待接诊") {
        QMessageBox::information(this, "请先叫号", "请先在候诊队列或本页点击“叫号”，再开始接诊。");
        return;
    }
```

Send:

```cpp
common::Request startRequest;
startRequest.module = "consultation";
startRequest.action = "start";
startRequest.payload["挂号单号"] = registrationNo;
apiClient()->send(startRequest);
```

Keep the dialog opening after this send for now; Task 5 will introduce response chaining for save/exam/prescription.

- [ ] **Step 6: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 4: Return Examination Patients to Follow-Up

**Files:**
- Modify: `server/src/modules/ExaminationService.cpp`
- Modify: `server/src/modules/RegistrationService.cpp`
- Modify: `server/src/modules/ConsultationService.cpp`

- [ ] **Step 1: Include workflow rules**

Add:

```cpp
#include "server/WorkflowRules.h"
```

to `server/src/modules/ExaminationService.cpp`.

- [ ] **Step 2: Use CHECKING when examination is requested**

Replace:

```cpp
UPDATE registrations SET status = 'CHECKING'
```

with:

```cpp
UPDATE registrations SET status = :checking
```

and bind:

```cpp
query.bindValue(":checking", WorkflowRules::statusAfterExaminationRequested());
```

- [ ] **Step 3: Use CHECK_DONE when examination is completed**

In `completeExaminationInDatabase()`, wrap the examination update and registration status update in a transaction:

```cpp
auto db = database->database();
if (!db.transaction()) {
    return {false, db.lastError().text(), {}};
}
QSqlQuery query(db);
```

After successful examination update, add:

```cpp
query.prepare("UPDATE registrations r "
              "JOIN examinations e ON e.registration_id = r.id "
              "SET r.status = :check_done "
              "WHERE e.examination_no = :examination_no AND r.status = :checking");
query.bindValue(":check_done", WorkflowRules::statusAfterExaminationCompleted());
query.bindValue(":checking", WorkflowRules::checking());
query.bindValue(":examination_no", examNo);
if (!query.exec()) {
    db.rollback();
    return {false, query.lastError().text(), {}};
}
if (!db.commit()) {
    return {false, db.lastError().text(), {}};
}
```

Return:

```cpp
return {true, "检查结果已回传，患者已进入检查完成待复诊。", {}};
```

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 5: Chain Consultation Follow-Up Requests Safely

**Files:**
- Modify: `client/include/client/pages/Pages.h`
- Modify: `client/src/pages/ConsultationPage.cpp`

- [ ] **Step 1: Add pending request state to `ConsultationPage`**

In `client/include/client/pages/Pages.h`, add private fields to `ConsultationPage`:

```cpp
    QJsonObject m_pendingExamRequest;
    QJsonObject m_pendingPrescriptionRequest;
```

- [ ] **Step 2: Connect response handler**

Add private slot declaration:

```cpp
    void onConsultationResponse(const common::Response& response);
```

In constructor:

```cpp
connect(apiClient, &ApiClient::responseReceived, this, &ConsultationPage::onConsultationResponse);
```

- [ ] **Step 3: Store exam/prescription requests instead of sending immediately**

Replace direct sends:

```cpp
apiClient()->send(examRequest);
apiClient()->send(prescriptionRequest);
```

with:

```cpp
m_pendingExamRequest = examRequest.payload;
m_pendingPrescriptionRequest = prescriptionRequest.payload;
```

Clear both before preparing new save:

```cpp
m_pendingExamRequest = {};
m_pendingPrescriptionRequest = {};
```

- [ ] **Step 4: Send follow-up only after consultation save succeeds**

Implement:

```cpp
void ConsultationPage::onConsultationResponse(const common::Response& response)
{
    if (response.data.value("module").toString() != "consultation") {
        return;
    }
    const QString action = response.data.value("action").toString();
    if (action != "save" && action != "saveWaiting") {
        return;
    }
    if (!response.success) {
        m_pendingExamRequest = {};
        m_pendingPrescriptionRequest = {};
        return;
    }

    if (!m_pendingExamRequest.isEmpty()) {
        common::Request examRequest;
        examRequest.module = "examination";
        examRequest.action = "create";
        examRequest.payload = m_pendingExamRequest;
        m_pendingExamRequest = {};
        apiClient()->send(examRequest);
    }

    if (!m_pendingPrescriptionRequest.isEmpty()) {
        common::Request prescriptionRequest;
        prescriptionRequest.module = "prescription";
        prescriptionRequest.action = "create";
        prescriptionRequest.payload = m_pendingPrescriptionRequest;
        m_pendingPrescriptionRequest = {};
        apiClient()->send(prescriptionRequest);
    }
}
```

- [ ] **Step 5: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 6: Enforce Payment Before Dispensing

**Files:**
- Modify: `server/src/modules/PrescriptionService.cpp`
- Modify: `client/src/pages/PrescriptionPage.cpp`

- [ ] **Step 1: Add server-side payment check**

In `dispensePrescriptionInDatabase()`, after verifying `status == "REVIEWED"`, add:

```cpp
query.prepare("SELECT COALESCE(b.status, '') "
              "FROM prescriptions pr "
              "JOIN registrations r ON r.id = pr.registration_id "
              "JOIN bills b ON b.registration_id = r.id "
              "WHERE pr.id = :prescription_id LIMIT 1");
query.bindValue(":prescription_id", prescriptionId);
if (!query.exec() || !query.next()) {
    db.rollback();
    return {false, "未找到该处方对应账单，不能发药。", {}};
}
const QString billStatus = query.value(0).toString();
query.finish();
if (billStatus != "PAID") {
    db.rollback();
    return {false, "该处方对应账单未缴费，不能发药。请先完成收费结算。", {}};
}
```

- [ ] **Step 2: Update dispense confirmation copy**

In `client/src/pages/PrescriptionPage.cpp`, replace:

```cpp
"确定已完成收费并向患者发药吗？发药后将扣减库存。"
```

with:

```cpp
"系统将校验账单是否已缴费；确认发药后会扣减库存。"
```

- [ ] **Step 3: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 7: Prevent Unsafe Generic Edits

**Files:**
- Modify: `client/src/pages/ModulePage.cpp`
- Modify: `server/src/modules/BillingService.cpp`

- [ ] **Step 1: Disable generic edit for workflow-controlled rows**

In `ModulePage::supportsEdit()`, remove `registration` and `billing` from the generic edit list:

```cpp
    return m_module == "patient"
        || m_module == "schedule"
        || m_module == "doctor"
        || m_module == "inventory"
        || m_module == "patientRecord";
```

- [ ] **Step 2: Keep billing changes through action buttons**

Do not remove `BillingPage::paySelectedBill()` or `BillingPage::refundSelectedBill()`. Those remain the only normal billing mutations.

- [ ] **Step 3: Harden billing update endpoint**

In `updateBill()`, reject status changes that are not amount maintenance:

```cpp
const QString status = payload.value("状态").toString().trimmed();
if (status == "PAID" || status == "REFUNDED" || status == "CANCELLED") {
    return {false, "账单状态请通过收费、退费或取消流程变更，不能直接编辑。", {}};
}
```

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure
```

Expected: PASS.

---

### Task 8: Final Verification

**Files:**
- No source changes unless verification exposes a defect.

- [ ] **Step 1: Full test suite**

Run:

```powershell
ctest --test-dir build-windows --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 2: Full build**

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: build exits with code 0.

- [ ] **Step 3: Server smoke test**

Run:

```powershell
$env:Path = "D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:Path"
$p = Start-Process -FilePath "D:\bs\HospitalOutpatientSystem\build-windows\server\hospital_server.exe" -ArgumentList "config\server.example.ini" -WorkingDirectory "D:\bs\HospitalOutpatientSystem" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
$alive = -not $p.HasExited
if ($alive) { Stop-Process -Id $p.Id -Force }
"server_alive=$alive"
```

Expected: `server_alive=True`.

- [ ] **Step 4: Manual UI smoke checklist**

Verify manually:

- New patient appointment appears in `候诊队列`.
- It does not appear in `医生接诊` until called.
- Calling the patient makes it appear in `医生接诊`.
- Starting consultation changes the row to `接诊中`.
- Applying examination changes the row to `检查中`.
- Completing examination changes the row to `检查完成待复诊`.
- Doctor follow-up can finish the visit.
- Prescription cannot be dispensed before billing is paid.
- After payment, reviewed prescription can be dispensed and inventory decreases.

- [ ] **Step 5: Workspace status**

Run:

```powershell
git status --short
```

Expected in this workspace: `fatal: not a git repository...`. Record changed files in final response instead of committing.

---

## Self-Review

- Coverage: The plan addresses queue/consultation duplication, direct start without call, examination follow-up ambiguity, unsafe chained requests, unpaid dispensing, unsafe generic edits, and missing workflow tests.
- Scope boundary: This plan does not redesign database schema or add a full appointment-payment subsystem. It keeps `registrations.status` as `VARCHAR` to avoid migration risk and because the existing schema already supports new status values.
- Type consistency: Status constants use `WorkflowRules` everywhere. UI display text is derived from SQL CASE clauses that match those constants.
- No placeholders: Every task names concrete files, concrete code snippets, commands, and expected results.
