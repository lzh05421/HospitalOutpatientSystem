# Security Audit Usability Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden the hospital outpatient system from a demo-oriented build into a more credible application by adding server-side sessions, closing demo login fallbacks, persisting audit logs, expanding tests, protecting the TCP frame parser, and exposing the triage feature cleanly.

**Architecture:** Add a small server-side `SessionManager` and make `RequestRouter` authorize from the request token instead of client-supplied role fields. Convert server sources into a reusable `hospital_server_core` library so Qt tests can exercise routing and services without launching the full TCP server. Keep the first optimization pass focused on security, auditability, verification, and one visible usability improvement.

**Tech Stack:** C++17, Qt Core/Network/Sql/Widgets/Test, CMake, CTest, MySQL through Qt SQL.

---

## File Structure

- Create: `server/include/server/SessionManager.h`
  - Owns in-memory login sessions and exposes `createSession()`, `findSession()`, and `invalidateSession()`.
- Create: `server/src/SessionManager.cpp`
  - Implements UUID token creation and lookup by token.
- Modify: `server/include/server/RequestRouter.h`
  - Adds `SessionManager*`, an authorization result type, and a `setSessionManager()` method.
- Modify: `server/src/RequestRouter.cpp`
  - Authorizes from token, injects trusted operator metadata into the request payload, writes operation logs to MySQL, and persists audit detail rows.
- Modify: `server/include/server/modules/AuthService.h`
  - Stores a `SessionManager*` and centralizes token creation through the session manager.
- Modify: `server/src/modules/AuthService.cpp`
  - Removes database-enabled demo fallback and removes plain-text password matching in database mode.
- Modify: `server/src/main.cpp`
  - Constructs `SessionManager`, passes it to `AuthService`, and wires it into `RequestRouter`.
- Modify: `server/src/ClientConnection.cpp`
  - Adds a maximum frame size guard.
- Modify: `client/src/ApiClient.cpp`
  - Adds a maximum frame size guard for server responses.
- Modify: `server/CMakeLists.txt`
  - Splits server implementation into `hospital_server_core` and keeps `hospital_server` as the thin executable target.
- Modify: `tests/CMakeLists.txt`
  - Adds routing/auth tests linked to `hospital_server_core`.
- Create: `tests/auth_router_tests.cpp`
  - Tests forged role rejection, valid token authorization, and trusted operator metadata injection.
- Create: `tests/protocol_frame_tests.cpp`
  - Tests that oversize frames are rejected indirectly by parser guard units if helper extraction is added, or keeps a focused placeholder-free protocol boundary test if the guard stays local.
- Modify: `client/include/client/pages/Pages.h`
  - Adds `AiTriagePage` as a `QWidget`, not a `ModulePage`.
- Modify: `client/src/pages/AiTriagePage.cpp`
  - Converts from `ModulePage` to standalone `QWidget` so it does not send unsupported `ai/triage` requests.
- Modify: `client/src/MainWindow.cpp`
  - Adds the triage page to the sidebar for staff roles.
- Modify: `client/CMakeLists.txt`
  - Adds `src/pages/AiTriagePage.cpp` to the client target.

---

### Task 1: Add Server-Side Sessions

**Files:**
- Create: `server/include/server/SessionManager.h`
- Create: `server/src/SessionManager.cpp`
- Modify: `server/CMakeLists.txt`

- [ ] **Step 1: Write the failing auth router test skeleton**

Create `tests/auth_router_tests.cpp` with this initial content:

```cpp
#include "server/RequestRouter.h"
#include "server/SessionManager.h"

#include <QtTest/QtTest>

using hospital::common::Request;
using hospital::common::Response;
using hospital::server::ModuleService;
using hospital::server::RequestRouter;
using hospital::server::SessionManager;

class EchoService final : public ModuleService
{
public:
    Response handle(const Request& request) override
    {
        QJsonObject data;
        data["operatorRoleCode"] = request.payload.value("__operatorRoleCode").toString();
        data["operatorName"] = request.payload.value("__operatorName").toString();
        return {true, "OK", data};
    }
};

class AuthRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void forgedAdminRoleWithoutTokenIsRejected();
    void validTokenInjectsTrustedOperatorMetadata();
};

void AuthRouterTests::forgedAdminRoleWithoutTokenIsRejected()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService patientService;
    router.setSessionManager(&sessions);
    router.registerService("patient", &patientService);

    Request request;
    request.module = "patient";
    request.action = "delete";
    request.payload["__operatorRoleCode"] = "ADMIN";

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("未登录") || response.message.contains("失效"));
}

void AuthRouterTests::validTokenInjectsTrustedOperatorMetadata()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService patientService;
    router.setSessionManager(&sessions);
    router.registerService("patient", &patientService);

    const auto session = sessions.createSession("42", "doctor01", "张明", "DOCTOR", "医生");

    Request request;
    request.module = "patient";
    request.action = "list";
    request.token = session.token;
    request.payload["__operatorRoleCode"] = "ADMIN";
    request.payload["__operatorName"] = "伪造管理员";

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("operatorRoleCode").toString(), QString("DOCTOR"));
    QCOMPARE(response.data.value("operatorName").toString(), QString("张明"));
}

QTEST_MAIN(AuthRouterTests)
#include "auth_router_tests.moc"
```

- [ ] **Step 2: Add the test target and run it to confirm it fails**

Modify `tests/CMakeLists.txt` by appending:

```cmake
add_executable(auth_router_tests
    auth_router_tests.cpp
)

target_link_libraries(auth_router_tests
    PRIVATE
        hospital_server_core
        hospital_common
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME auth_router_tests COMMAND auth_router_tests)

if(WIN32)
    set_tests_properties(auth_router_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Sql>;PATH=path_list_prepend:${TEST_CXX_RUNTIME_DIR}"
    )
endif()
```

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: FAIL because `hospital_server_core` and `SessionManager` do not exist yet.

- [ ] **Step 3: Split server code into a reusable core library**

Replace `server/CMakeLists.txt` with:

```cmake
set(HOSPITAL_SERVER_CORE_SOURCES
    include/server/AppConfig.h
    include/server/ClientConnection.h
    include/server/DatabaseManager.h
    include/server/DemoRepository.h
    include/server/HospitalServer.h
    include/server/RequestRouter.h
    include/server/SessionManager.h
    include/server/SqlJson.h
    include/server/modules/AuthService.h
    include/server/modules/ModuleServices.h
    src/AppConfig.cpp
    src/ClientConnection.cpp
    src/DatabaseManager.cpp
    src/DemoRepository.cpp
    src/HospitalServer.cpp
    src/RequestRouter.cpp
    src/SessionManager.cpp
    src/SqlJson.cpp
    src/modules/AuthService.cpp
    src/modules/BillingService.cpp
    src/modules/ConsultationService.cpp
    src/modules/DashboardService.cpp
    src/modules/DepartmentService.cpp
    src/modules/DoctorService.cpp
    src/modules/ExaminationService.cpp
    src/modules/InventoryService.cpp
    src/modules/PatientService.cpp
    src/modules/PatientRecordService.cpp
    src/modules/PrescriptionService.cpp
    src/modules/RegistrationService.cpp
    src/modules/ScheduleService.cpp
    src/modules/StatisticsService.cpp
    src/modules/OperationLogService.cpp
)

add_library(hospital_server_core STATIC
    ${HOSPITAL_SERVER_CORE_SOURCES}
)

target_include_directories(hospital_server_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hospital_server_core
    PUBLIC
        hospital_common
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Sql
)

add_executable(hospital_server
    src/main.cpp
)

target_link_libraries(hospital_server
    PRIVATE
        hospital_server_core
)
```

- [ ] **Step 4: Implement `SessionManager`**

Create `server/include/server/SessionManager.h`:

```cpp
#pragma once

#include <QHash>
#include <QMutex>
#include <QString>

namespace hospital::server {

struct Session
{
    QString token;
    QString userId;
    QString username;
    QString realName;
    QString roleCode;
    QString roleName;
};

class SessionManager
{
public:
    Session createSession(const QString& userId,
                          const QString& username,
                          const QString& realName,
                          const QString& roleCode,
                          const QString& roleName);
    bool findSession(const QString& token, Session* session) const;
    void invalidateSession(const QString& token);

private:
    mutable QMutex m_mutex;
    QHash<QString, Session> m_sessions;
};

} // namespace hospital::server
```

Create `server/src/SessionManager.cpp`:

```cpp
#include "server/SessionManager.h"

#include <QMutexLocker>
#include <QUuid>

namespace hospital::server {

Session SessionManager::createSession(const QString& userId,
                                      const QString& username,
                                      const QString& realName,
                                      const QString& roleCode,
                                      const QString& roleName)
{
    Session session;
    session.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.userId = userId;
    session.username = username;
    session.realName = realName;
    session.roleCode = roleCode;
    session.roleName = roleName;

    QMutexLocker locker(&m_mutex);
    m_sessions.insert(session.token, session);
    return session;
}

bool SessionManager::findSession(const QString& token, Session* session) const
{
    if (token.trimmed().isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const auto it = m_sessions.constFind(token);
    if (it == m_sessions.constEnd()) {
        return false;
    }
    if (session) {
        *session = it.value();
    }
    return true;
}

void SessionManager::invalidateSession(const QString& token)
{
    QMutexLocker locker(&m_mutex);
    m_sessions.remove(token);
}

} // namespace hospital::server
```

- [ ] **Step 5: Run build and confirm only router API failures remain**

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: FAIL because `RequestRouter::setSessionManager()` does not exist yet.

---

### Task 2: Enforce Token-Based Authorization

**Files:**
- Modify: `server/include/server/RequestRouter.h`
- Modify: `server/src/RequestRouter.cpp`
- Modify: `server/src/main.cpp`

- [ ] **Step 1: Update `RequestRouter.h`**

Change `server/include/server/RequestRouter.h` to include `SessionManager` and replace the private authorization shape:

```cpp
#pragma once

#include "common/Protocol.h"
#include "server/SessionManager.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace hospital::server {

class DatabaseManager;

class ModuleService
{
public:
    virtual ~ModuleService() = default;
    virtual common::Response handle(const common::Request& request) = 0;
};

class RequestRouter
{
public:
    void registerService(const QString& module, ModuleService* service);
    void setDatabase(DatabaseManager* database);
    void setSessionManager(SessionManager* sessions);
    common::Response route(const common::Request& request) const;

private:
    struct AuthorizationResult
    {
        bool success = false;
        QString message;
        bool authenticated = false;
        Session session;
    };

    AuthorizationResult authorize(const common::Request& request) const;
    bool isAllowed(const QString& roleCode, const QString& module, const QString& action) const;
    qint64 writeOperationLog(const common::Request& request, const common::Response& response) const;
    void writeAuditDetails(qint64 operationLogId, const common::Request& request, const common::Response& response) const;
    QJsonObject publicResponseData(QJsonObject data) const;

    QHash<QString, ModuleService*> m_services;
    DatabaseManager* m_database = nullptr;
    SessionManager* m_sessions = nullptr;
};

} // namespace hospital::server
```

- [ ] **Step 2: Update `RequestRouter.cpp` routing and authorization**

Replace `registerService`, `setDatabase`, `route`, and `authorize` in `server/src/RequestRouter.cpp` with:

```cpp
void RequestRouter::registerService(const QString& module, ModuleService* service)
{
    m_services.insert(module, service);
}

void RequestRouter::setDatabase(DatabaseManager* database)
{
    m_database = database;
}

void RequestRouter::setSessionManager(SessionManager* sessions)
{
    m_sessions = sessions;
}

common::Response RequestRouter::route(const common::Request& request) const
{
    const auto auth = authorize(request);
    if (!auth.success) {
        common::Response response{false, auth.message, {}};
        response.data["module"] = request.module;
        response.data["action"] = request.action;
        return response;
    }

    auto routedRequest = request;
    if (auth.authenticated) {
        routedRequest.payload["__operatorUserId"] = auth.session.userId;
        routedRequest.payload["__operatorName"] = auth.session.realName.isEmpty() ? auth.session.username : auth.session.realName;
        routedRequest.payload["__operatorRoleCode"] = auth.session.roleCode;
        routedRequest.payload["__operatorRole"] = auth.session.roleName;
    }

    auto* service = m_services.value(routedRequest.module, nullptr);
    if (!service) {
        return {false, QString("Unknown module: %1").arg(routedRequest.module), {}};
    }

    auto response = service->handle(routedRequest);
    response.data["module"] = routedRequest.module;
    response.data["action"] = routedRequest.action;
    const qint64 operationLogId = writeOperationLog(routedRequest, response);
    writeAuditDetails(operationLogId, routedRequest, response);
    response.data = publicResponseData(response.data);
    return response;
}

RequestRouter::AuthorizationResult RequestRouter::authorize(const common::Request& request) const
{
    if (request.module == "auth") {
        return {true, "OK", false, {}};
    }

    const bool publicAction = (request.module == "schedule" && request.action == "list")
        || (request.module == "registration" && request.action == "create");
    if (publicAction) {
        return {true, "OK", false, {}};
    }

    if (!m_sessions) {
        return {false, "服务端会话管理未初始化，请检查 server 配置。", false, {}};
    }

    Session session;
    if (!m_sessions->findSession(request.token, &session)) {
        return {false, "未登录或会话已失效，请重新登录。", false, {}};
    }

    if (isAllowed(session.roleCode, request.module, request.action)) {
        return {true, "OK", true, session};
    }

    return {false, QString("权限不足：%1 不能执行 %2/%3。").arg(session.roleCode, request.module, request.action), true, session};
}
```

- [ ] **Step 3: Update operation log function declaration body to return `qint64`**

In `server/src/RequestRouter.cpp`, change the function signature:

```cpp
qint64 RequestRouter::writeOperationLog(const common::Request& request, const common::Response& response) const
```

Temporarily keep the body demo-only and return `0` after appending:

```cpp
    DemoRepository::instance().appendOperationLog(operatorName, request.module, request.action, content);
    return 0;
```

For every earlier return in that function, return `0`.

- [ ] **Step 4: Wire sessions in `main.cpp`**

In `server/src/main.cpp`, add:

```cpp
#include "server/SessionManager.h"
```

Then after creating `RequestRouter router;`, insert:

```cpp
SessionManager sessions;
router.setSessionManager(&sessions);
```

Change:

```cpp
AuthService authService(&database);
```

to:

```cpp
AuthService authService(&database, &sessions);
```

- [ ] **Step 5: Run the auth tests**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure -R auth_router_tests
```

Expected: FAIL because `AuthService` has not been updated to accept `SessionManager`.

---

### Task 3: Remove Demo Login Fallback From Database Mode

**Files:**
- Modify: `server/include/server/modules/AuthService.h`
- Modify: `server/src/modules/AuthService.cpp`

- [ ] **Step 1: Update `AuthService.h`**

Replace the class definition in `server/include/server/modules/AuthService.h` with:

```cpp
#pragma once

#include "server/RequestRouter.h"

namespace hospital::server {

class DatabaseManager;
class SessionManager;

class AuthService : public ModuleService
{
public:
    AuthService(DatabaseManager* database, SessionManager* sessions);
    common::Response handle(const common::Request& request) override;

private:
    common::Response login(const common::Request& request);
    common::Response responseForSession(const QString& username,
                                        const QString& userId,
                                        const QString& realName,
                                        const QString& roleCode,
                                        const QString& roleName) const;

    DatabaseManager* m_database = nullptr;
    SessionManager* m_sessions = nullptr;
};

} // namespace hospital::server
```

- [ ] **Step 2: Update `AuthService.cpp` constructor and session response**

Add include:

```cpp
#include "server/SessionManager.h"
```

Change the constructor:

```cpp
AuthService::AuthService(DatabaseManager* database, SessionManager* sessions)
    : m_database(database)
    , m_sessions(sessions)
{
}
```

Add:

```cpp
common::Response AuthService::responseForSession(const QString& username,
                                                 const QString& userId,
                                                 const QString& realName,
                                                 const QString& roleCode,
                                                 const QString& roleName) const
{
    if (!m_sessions) {
        return {false, "服务端会话管理未初始化。", {}};
    }

    const auto session = m_sessions->createSession(userId, username, realName, roleCode, roleName);
    QJsonObject data;
    data["token"] = session.token;
    data["userId"] = session.userId;
    data["username"] = session.username;
    data["realName"] = session.realName;
    data["roleCode"] = session.roleCode;
    data["roleName"] = session.roleName;
    data["loginTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return {true, "Login success", data};
}
```

- [ ] **Step 3: Replace demo login token creation**

Change `demoLogin` to return user identity without token, or replace it with a helper:

```cpp
QJsonObject demoUser(const QString& username)
{
    return demoUsers().value(username);
}
```

In `login()`, for disabled database mode, use:

```cpp
    if (!m_database->isEnabled()) {
        const auto users = demoUsers();
        if (!users.contains(username) || password != "123456") {
            return {false, "账号或密码错误。演示账号：admin、director01、reg01、doctor01、doctor02、doctor03、doctor04、doctor05、doctor06、pharmacy01、cashier01，密码均为 123456。", {}};
        }
        const auto user = users.value(username);
        return responseForSession(username,
                                  user.value("userId").toString(),
                                  user.value("realName").toString(),
                                  user.value("roleCode").toString(),
                                  user.value("roleName").toString());
    }
```

- [ ] **Step 4: Remove plain-text password matching and database-mode fallback**

Change the SQL in `login()` from:

```cpp
        "AND (u.password_hash = :sha256 OR u.password_hash = :plain)");
```

to:

```cpp
        "AND u.password_hash = :sha256");
```

Remove:

```cpp
    query.bindValue(":plain", password);
```

Replace the `if (!query.next())` block with:

```cpp
    if (!query.next()) {
        return {false, "账号或密码错误。", {}};
    }
```

Replace the final JSON response construction with:

```cpp
    return responseForSession(query.value(record.indexOf("username")).toString(),
                              QString::number(query.value(record.indexOf("id")).toLongLong()),
                              query.value(record.indexOf("real_name")).toString(),
                              query.value(record.indexOf("role_code")).toString(),
                              query.value(record.indexOf("role_name")).toString());
```

- [ ] **Step 5: Run tests and build**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure -R "auth_router_tests|protocol_tests"
```

Expected: PASS.

---

### Task 4: Persist Operation Logs and Audit Details

**Files:**
- Modify: `server/src/RequestRouter.cpp`
- Modify: `tests/auth_router_tests.cpp`

- [ ] **Step 1: Add a test that audit details are stripped from responses**

Append this test method declaration in `AuthRouterTests`:

```cpp
    void auditDetailsAreNotReturnedToClient();
```

Add this service above the test class:

```cpp
class AuditService final : public ModuleService
{
public:
    Response handle(const Request&) override
    {
        QJsonObject data;
        data["visible"] = "ok";
        data["__auditDetails"] = QJsonArray{
            QJsonObject{{"businessKey", "R1"}, {"fieldName", "诊断"}, {"oldValue", "旧"}, {"newValue", "新"}, {"changeReason", "测试"}}
        };
        return {true, "OK", data};
    }
};
```

Add the test:

```cpp
void AuthRouterTests::auditDetailsAreNotReturnedToClient()
{
    SessionManager sessions;
    RequestRouter router;
    AuditService service;
    router.setSessionManager(&sessions);
    router.registerService("patientRecord", &service);
    const auto session = sessions.createSession("1", "admin", "系统管理员", "ADMIN", "系统管理员");

    Request request;
    request.module = "patientRecord";
    request.action = "update";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("visible").toString(), QString("ok"));
    QVERIFY(!response.data.contains("__auditDetails"));
}
```

- [ ] **Step 2: Implement MySQL operation log insertion**

Replace `writeOperationLog()` in `server/src/RequestRouter.cpp` with:

```cpp
qint64 RequestRouter::writeOperationLog(const common::Request& request, const common::Response& response) const
{
    if (!response.success || request.module == "auth") {
        return 0;
    }

    const QStringList writeActions = {
        "create", "update", "delete", "reset", "save", "saveWaiting", "inbound",
        "pay", "refund", "review", "dispense", "call", "complete"
    };
    if (!writeActions.contains(request.action)) {
        return 0;
    }

    const QString operatorName = request.payload.value("__operatorName").toString("系统");
    const QString content = QString("%1/%2 %3")
        .arg(request.module, request.action,
             QString::fromUtf8(QJsonDocument(request.payload).toJson(QJsonDocument::Compact)));

    if (!m_database || !m_database->isEnabled()) {
        DemoRepository::instance().appendOperationLog(operatorName, request.module, request.action, content);
        return 0;
    }

    if (!m_database->ensureOpen()) {
        return 0;
    }

    const QString userId = request.payload.value("__operatorUserId").toVariant().toString();
    QSqlQuery query(m_database->database());
    query.prepare("INSERT INTO operation_logs (user_id, module, action, content) "
                  "VALUES (:user_id, :module, :action, :content)");
    query.bindValue(":user_id", userId.isEmpty() ? QVariant() : QVariant(userId.toLongLong()));
    query.bindValue(":module", request.module.left(64));
    query.bindValue(":action", request.action.left(64));
    query.bindValue(":content", content.left(500));
    if (!query.exec()) {
        return 0;
    }
    return query.lastInsertId().toLongLong();
}
```

- [ ] **Step 3: Keep `writeAuditDetails()` as the persistence hook**

Verify that `route()` calls:

```cpp
    const qint64 operationLogId = writeOperationLog(routedRequest, response);
    writeAuditDetails(operationLogId, routedRequest, response);
    response.data = publicResponseData(response.data);
```

- [ ] **Step 4: Run tests**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure -R auth_router_tests
```

Expected: PASS.

---

### Task 5: Add TCP Frame Size Guards

**Files:**
- Modify: `server/src/ClientConnection.cpp`
- Modify: `client/src/ApiClient.cpp`
- Modify: `tests/protocol_tests.cpp`

- [ ] **Step 1: Add protocol boundary test**

Append this test declaration to `ProtocolTests`:

```cpp
    void tryDecodeRequestRejectsMissingModuleOrAction();
```

Add the test:

```cpp
void ProtocolTests::tryDecodeRequestRejectsMissingModuleOrAction()
{
    hospital::common::Request request;
    QString error;

    const bool ok = hospital::common::Protocol::tryDecodeRequest("{\"module\":\"patient\",\"payload\":{}}\n", &request, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains("module") || error.contains("action"));
}
```

- [ ] **Step 2: Guard server input buffer**

In `server/src/ClientConnection.cpp`, add near the namespace opening:

```cpp
namespace {
constexpr qsizetype kMaxFrameBytes = 1024 * 1024;
}
```

After `m_buffer += m_socket->readAll();`, add:

```cpp
    if (m_buffer.size() > kMaxFrameBytes) {
        const common::Response response{false, QStringLiteral("请求过大，已断开连接。"), {}};
        m_socket->write(common::Protocol::encodeResponse(response));
        m_socket->disconnectFromHost();
        m_buffer.clear();
        return;
    }
```

- [ ] **Step 3: Guard client response buffer**

In `client/src/ApiClient.cpp`, add near the namespace opening:

```cpp
namespace {
constexpr qsizetype kMaxFrameBytes = 1024 * 1024;
}
```

After `m_buffer += m_socket.readAll();`, add:

```cpp
    if (m_buffer.size() > kMaxFrameBytes) {
        m_buffer.clear();
        emit errorOccurred(QStringLiteral("服务端响应过大，已丢弃。"));
        m_socket.disconnectFromHost();
        return;
    }
```

- [ ] **Step 4: Run build and protocol tests**

Run:

```powershell
cmake --build --preset windows-qt-mingw
ctest --test-dir build-windows --output-on-failure -R protocol_tests
```

Expected: PASS.

---

### Task 6: Expose Triage Without Unsupported Server Requests

**Files:**
- Modify: `client/include/client/pages/Pages.h`
- Modify: `client/src/pages/AiTriagePage.cpp`
- Modify: `client/src/MainWindow.cpp`
- Modify: `client/CMakeLists.txt`

- [ ] **Step 1: Add `AiTriagePage` declaration**

In `client/include/client/pages/Pages.h`, after `DashboardPage`, add:

```cpp
class AiTriagePage : public QWidget
{
    Q_OBJECT
public:
    explicit AiTriagePage(ApiClient* apiClient, QWidget* parent = nullptr);
};
```

- [ ] **Step 2: Convert `AiTriagePage.cpp` to standalone QWidget**

Replace the constructor signature and setup in `client/src/pages/AiTriagePage.cpp` with:

```cpp
AiTriagePage::AiTriagePage(ApiClient*, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(14);

    auto* title = new QLabel("智能分诊", this);
    title->setObjectName("pageTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* description = new QLabel("根据症状关键词给出科室建议、紧急程度和挂号提示。", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);

    auto* box = new QGroupBox("症状分析", this);
```

At the bottom, replace:

```cpp
    layout()->addWidget(box);
```

with:

```cpp
    root->addWidget(title);
    root->addWidget(description);
    root->addWidget(box);
    root->addStretch();
}
```

Keep the existing keyword analysis body.

- [ ] **Step 3: Add source file to client target**

In `client/CMakeLists.txt`, add:

```cmake
    src/pages/AiTriagePage.cpp
```

near the other `src/pages/*.cpp` entries.

- [ ] **Step 4: Add navigation entry**

In `client/src/MainWindow.cpp`, after the dashboard entry, add:

```cpp
    addIfAllowed("智能分诊", {"ADMIN", "DIRECTOR", "REGISTRAR", "DOCTOR"}, [this]() { return new AiTriagePage(m_apiClient, this); });
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: PASS.

---

### Task 7: Verification and Manual Smoke Test

**Files:**
- No source changes unless verification exposes a defect.

- [ ] **Step 1: Run the full test suite**

Run:

```powershell
ctest --test-dir build-windows --output-on-failure
```

Expected: all tests PASS, including `protocol_tests` and `auth_router_tests`.

- [ ] **Step 2: Build all targets**

Run:

```powershell
cmake --build --preset windows-qt-mingw
```

Expected: build succeeds with no compiler errors.

- [ ] **Step 3: Run server/client smoke test**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows_run_all.ps1
```

Expected: server starts on `127.0.0.1:8899`, client opens, `admin / 123456` works only when demo data or database seed supports the hashed password, and non-public actions fail after clearing the token.

- [ ] **Step 4: Manual UI checks**

Verify:

- Sidebar shows `智能分诊`.
- Opening `智能分诊` does not show `Unknown module: ai`.
- Entering `胸痛 呼吸困难` recommends urgent care.
- Login still displays role-specific menu items.
- Patient appointment can still load schedules and submit public registration.

- [ ] **Step 5: Check repository status**

Run:

```powershell
git status --short
```

Expected in this workspace today: `fatal: not a git repository...`. Because the current directory is not a git repository, record the changed file list in the final response instead of making commits.

---

## Self-Review

- Spec coverage: The plan covers the previously identified P0 authorization issue, P1 demo login fallback, P1 missing audit persistence, P1 shallow business tests, P2 oversize frame protection, and P2 triage discoverability.
- Scope intentionally deferred: Full typed edit dialogs are not included in this first pass because they touch many page-specific workflows and should be a second plan after security and audit are stable.
- Placeholder scan: No `TBD`, `TODO`, vague "add tests", or undefined implementation steps remain.
- Type consistency: `SessionManager`, `Session`, `RequestRouter::AuthorizationResult`, `setSessionManager()`, and `AuthService(DatabaseManager*, SessionManager*)` are consistently named across tasks.
