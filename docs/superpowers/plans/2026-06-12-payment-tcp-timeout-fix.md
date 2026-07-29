# Payment TCP Timeout Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `PaymentSelectionDialog` receive a deterministic payment response or an immediate structured failure instead of waiting until `TimeoutException`.

**Architecture:** Keep the existing custom TCP Socket + JSON protocol. Add a lightweight request correlation id in request headers and response data, harden socket write failure detection, and guarantee server-side route failures are returned as framed JSON responses.

**Tech Stack:** C++17, Qt Widgets, QTcpSocket, QTimer, QJsonObject, existing `common::Protocol`.

---

## File Structure

- Modify: `client/src/ApiClient.cpp`
  - Validate `QTcpSocket::write()` return value and emit `errorOccurred` when the frame cannot be queued.
- Modify: `client/include/client/PaymentNetworkManager.h`
  - Store the pending payment request id.
- Modify: `client/src/PaymentNetworkManager.cpp`
  - Attach `X-Request-Id` to payment requests and match responses by `requestId`.
- Modify: `server/src/RequestRouter.cpp`
  - Add a response envelope helper so every routed response includes `module`, `action`, and `requestId`.
- Modify: `server/src/ClientConnection.cpp`
  - Catch route exceptions and write a structured error response instead of leaving the client pending.
- Test: `tests/payment_router_tests.cpp`
  - Verify `X-Request-Id` is echoed as `data.requestId`.
- Test: `tests/payment_security_source_tests.cpp`
  - Extend source coverage so future edits keep request correlation and non-blocking socket behavior.

---

### Task 1: Harden Socket Send

**Files:**
- Modify: `client/src/ApiClient.cpp:28-49`
- Test: `tests/payment_security_source_tests.cpp`

- [ ] **Step 1: Update source test expectations**

Add these fragments to the existing `protocolHeader + protocolSource + apiClient` check in `tests/payment_security_source_tests.cpp`:

```cpp
"const QByteArray frame = common::Protocol::encodeRequest(copy)",
"const qint64 written = m_socket.write(frame)",
"written != frame.size()",
"m_socket.flush()",
"请求写入失败"
```

- [ ] **Step 2: Replace `ApiClient::send()` write tail**

In `client/src/ApiClient.cpp`, replace:

```cpp
m_socket.write(common::Protocol::encodeRequest(copy));
return true;
```

with:

```cpp
const QByteArray frame = common::Protocol::encodeRequest(copy);
const qint64 written = m_socket.write(frame);
if (written != frame.size()) {
    emit errorOccurred(QStringLiteral("请求写入失败：%1").arg(m_socket.errorString()));
    return false;
}

m_socket.flush();
return true;
```

- [ ] **Step 3: Verify by build/test**

Run:

```powershell
cmake --build build-windows --target payment_security_source_tests
.\build-windows\tests\payment_security_source_tests.exe
```

Expected: process exits with code `0`.

---

### Task 2: Add Payment Request Correlation

**Files:**
- Modify: `client/include/client/PaymentNetworkManager.h:80-83`
- Modify: `client/src/PaymentNetworkManager.cpp:1-8`
- Modify: `client/src/PaymentNetworkManager.cpp:99-120`
- Modify: `client/src/PaymentNetworkManager.cpp:184-203`
- Test: `tests/payment_security_source_tests.cpp`

- [ ] **Step 1: Add include and pending field**

In `client/src/PaymentNetworkManager.cpp`, add:

```cpp
#include <QUuid>
```

In `client/include/client/PaymentNetworkManager.h`, add this private member near `m_pendingBillNo`:

```cpp
QString m_pendingRequestId;
```

- [ ] **Step 2: Attach request id before sending**

Replace `PaymentNetworkManager::sendRequest()` with:

```cpp
void PaymentNetworkManager::sendRequest(const common::Request& request, RequestKind kind, int timeoutMs)
{
    if (hasPendingRequest()) {
        fail(m_pendingKind, "已有支付请求正在处理中，请稍后再试。", "Duplicate payment request blocked", "DUPLICATE_REQUEST");
        return;
    }

    common::Request outbound = request;
    m_pendingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    outbound.headers["X-Request-Id"] = m_pendingRequestId;
    m_pendingKind = kind;
    m_pendingBillNo = outbound.payload.value("billNo").toString(outbound.payload.value("账单号").toString());
    qInfo().noquote()
        << "PaymentRequest"
        << "action=" + outbound.action
        << "requestId=" + m_pendingRequestId
        << "billNo=" + m_pendingBillNo
        << "connected=" + QString(m_apiClient->isConnected() ? "true" : "false")
        << "timeoutMs=" + QString::number(timeoutMs);
    m_timeoutTimer->start(timeoutMs);
    if (!m_apiClient->send(outbound)) {
        fail(kind, "支付请求发送失败，请检查服务端连接。", "ApiClient::send returned false", "SEND_FAILED");
    }
}
```

- [ ] **Step 3: Match payment response by request id**

At the top of `PaymentNetworkManager::onResponseReceived()`, replace the first `if` block with:

```cpp
if (m_pendingKind == RequestKind::None) {
    return;
}

const QString responseRequestId = response.data.value("requestId").toString();
if (!m_pendingRequestId.isEmpty() && responseRequestId != m_pendingRequestId) {
    return;
}

if (response.data.value("module").toString() != "billing") {
    return;
}
```

- [ ] **Step 4: Clear request id on finish**

Update `finishRequest()`:

```cpp
void PaymentNetworkManager::finishRequest()
{
    m_timeoutTimer->stop();
    m_medicalSlowTimer->stop();
    m_pendingKind = RequestKind::None;
    m_pendingBillNo.clear();
    m_pendingRequestId.clear();
}
```

- [ ] **Step 5: Update source test expectations**

Add these fragments to the payment network source check in `tests/payment_security_source_tests.cpp`:

```cpp
"QUuid",
"m_pendingRequestId",
"X-Request-Id",
"requestId="
```

- [ ] **Step 6: Verify by build/test**

Run:

```powershell
cmake --build build-windows --target payment_security_source_tests
.\build-windows\tests\payment_security_source_tests.exe
```

Expected: process exits with code `0`.

---

### Task 3: Echo Request Id From Router

**Files:**
- Modify: `server/src/RequestRouter.cpp:20-35`
- Modify: `server/src/RequestRouter.cpp:43-113`
- Test: `tests/payment_router_tests.cpp`

- [ ] **Step 1: Add envelope helper**

Add this helper inside the anonymous namespace in `server/src/RequestRouter.cpp`:

```cpp
common::Response withEnvelope(common::Response response, const common::Request& request)
{
    response.data["module"] = request.module;
    response.data["action"] = request.action;
    const QString requestId = request.headers.value("X-Request-Id").toString().trimmed();
    if (!requestId.isEmpty()) {
        response.data["requestId"] = requestId;
    }
    return response;
}
```

- [ ] **Step 2: Use helper for auth failure**

Replace the auth failure branch:

```cpp
if (!auth.success) {
    common::Response response{false, auth.message, {}};
    response.data["module"] = request.module;
    response.data["action"] = request.action;
    return response;
}
```

with:

```cpp
if (!auth.success) {
    return withEnvelope({false, auth.message, {}}, request);
}
```

- [ ] **Step 3: Use helper for unknown service and normal route**

Replace:

```cpp
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
```

with:

```cpp
if (!service) {
    return withEnvelope({false, QString("Unknown module: %1").arg(routedRequest.module), {}}, routedRequest);
}

auto response = withEnvelope(service->handle(routedRequest), routedRequest);
const qint64 operationLogId = writeOperationLog(routedRequest, response);
writeAuditDetails(operationLogId, routedRequest, response);
response.data = publicResponseData(response.data);
return response;
```

- [ ] **Step 4: Add router test assertion**

In `tests/payment_router_tests.cpp`, add this request header before routing:

```cpp
request.headers["X-Request-Id"] = "pay-qr-test-001";
```

Then add this assertion after the existing action assertion:

```cpp
QCOMPARE(response.data.value("requestId").toString(), QString("pay-qr-test-001"));
```

- [ ] **Step 5: Verify by build/test**

Run:

```powershell
cmake --build build-windows --target payment_router_tests
.\build-windows\tests\payment_router_tests.exe
```

Expected: process exits with code `0`.

---

### Task 4: Return Structured Errors From ClientConnection

**Files:**
- Modify: `server/src/ClientConnection.cpp:1-6`
- Modify: `server/src/ClientConnection.cpp:50-51`
- Test: `tests/payment_security_source_tests.cpp`

- [ ] **Step 1: Include exception handling support**

In `server/src/ClientConnection.cpp`, add:

```cpp
#include <exception>
```

- [ ] **Step 2: Wrap route and write**

Replace:

```cpp
const auto response = m_router->route(request);
m_socket->write(common::Protocol::encodeResponse(response));
```

with:

```cpp
try {
    const auto response = m_router->route(request);
    m_socket->write(common::Protocol::encodeResponse(response));
    m_socket->flush();
} catch (const std::exception& ex) {
    common::Response response{false, QStringLiteral("服务端处理异常：%1").arg(ex.what()), {}};
    response.data["module"] = request.module;
    response.data["action"] = request.action;
    const QString requestId = request.headers.value("X-Request-Id").toString().trimmed();
    if (!requestId.isEmpty()) {
        response.data["requestId"] = requestId;
    }
    m_socket->write(common::Protocol::encodeResponse(response));
    m_socket->flush();
}
```

- [ ] **Step 3: Update source test expectations**

Add a `ClientConnection.cpp` read in `tests/payment_security_source_tests.cpp`:

```cpp
const std::string clientConnection = readFile("server/src/ClientConnection.cpp");
```

Add this check:

```cpp
if (!containsAll(clientConnection, {
        "try {",
        "catch (const std::exception& ex)",
        "服务端处理异常",
        "X-Request-Id",
        "m_socket->flush()"
    })) {
    return 7;
}
```

- [ ] **Step 4: Verify by build/test**

Run:

```powershell
cmake --build build-windows --target payment_security_source_tests
.\build-windows\tests\payment_security_source_tests.exe
```

Expected: process exits with code `0`.

---

### Task 5: Final Regression Verification

**Files:**
- No code changes.

- [ ] **Step 1: Build affected tests**

Run:

```powershell
cmake --build build-windows --target payment_security_source_tests payment_router_tests protocol_tests
```

Expected: build exits with code `0`.

- [ ] **Step 2: Run affected tests**

Run:

```powershell
.\build-windows\tests\payment_security_source_tests.exe
.\build-windows\tests\payment_router_tests.exe
.\build-windows\tests\protocol_tests.exe
```

Expected: each executable exits with code `0`.

- [ ] **Step 3: Manual static checklist**

Confirm these conditions in code:

```text
client/src/ApiClient.cpp:
- send() checks write return value.
- send() does not call waitForBytesWritten().

client/src/PaymentNetworkManager.cpp:
- sendRequest() writes X-Request-Id.
- onResponseReceived() ignores unrelated requestId values.
- finishRequest() clears m_pendingRequestId.

server/src/RequestRouter.cpp:
- auth failure, unknown module, and normal service response all include envelope metadata.

server/src/ClientConnection.cpp:
- route exceptions are converted to encoded JSON responses.
- no waitForReadyRead() or waitForBytesWritten() is introduced.
```

---

## Self-Review

**Spec coverage:** The plan addresses the three static-analysis findings: non-blocking send failure detection, payment response correlation, and guaranteed structured server errors. It preserves the custom TCP Socket + JSON protocol and does not introduce HTTP or `QNetworkAccessManager`.

**Placeholder scan:** No `TBD`, `TODO`, or vague implementation steps remain.

**Type consistency:** The request correlation uses existing `Request::headers` and `Response::data`, so no protocol struct migration is required. `m_pendingRequestId` is consistently a `QString`.
