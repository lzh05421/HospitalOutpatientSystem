#include "server/RequestRouter.h"
#include "server/DatabaseManager.h"
#include "server/modules/AuthService.h"
#include "server/SessionManager.h"

#include <QJsonArray>
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
        data["patientId"] = request.payload.value("__patientId").toVariant().toString();
        data["currentUserType"] = request.payload.value("__currentUserType").toString();
        data["permissions"] = request.payload.value("__permissions").toArray();
        return {true, "OK", data};
    }
};

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

class AuthRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void forgedAdminRoleWithoutTokenIsRejected();
    void validTokenInjectsTrustedOperatorMetadata();
    void anonymousBillingPayWithoutPaymentTokenIsRejected();
    void billingPayWithPaymentTokenStillRequiresLogin();
    void registrationCreateRequiresLogin();
    void registrationHistoryRequiresLogin();
    void authorizationHeaderBearerTokenAuthenticatesRequest();
    void patientTokenCanCreateRegistrationWithPatientIdentity();
    void patientTokenCanReadOwnHistoryWithPatientIdentity();
    void patientTokenCanConfirmOwnPaymentWithPatientIdentity();
    void auditDetailsAreNotReturnedToClient();
    void directorDemoAccountLogsInWithDefaultPassword();
    void adminWithoutExplicitDoctorCreatePermissionIsRejected();
    void adminWithEmptyPermissionListCannotFallBackToDoctorCreate();
    void adminCanAlwaysReadOperationLogForRecovery();
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

    const auto session = sessions.createSession("42",
                                                "doctor01",
                                                "张明",
                                                "DOCTOR",
                                                "医生",
                                                QStringList{"patient:list"});

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

void AuthRouterTests::anonymousBillingPayWithoutPaymentTokenIsRejected()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService billingService;
    router.setSessionManager(&sessions);
    router.registerService("billing", &billingService);

    Request request;
    request.module = "billing";
    request.action = "pay";
    request.payload["账单号"] = "B202606110001";

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("未登录") || response.message.contains("失效"));
}

void AuthRouterTests::billingPayWithPaymentTokenStillRequiresLogin()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService billingService;
    router.setSessionManager(&sessions);
    router.registerService("billing", &billingService);

    Request request;
    request.module = "billing";
    request.action = "pay";
    request.payload["账单号"] = "B202606110001";
    request.payload["paymentToken"] = "patient-payment-token";
    request.payload["__operatorRoleCode"] = "ADMIN";
    request.payload["__operatorName"] = "伪造管理员";

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("未登录") || response.message.contains("失效"));
}

void AuthRouterTests::registrationCreateRequiresLogin()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService registrationService;
    router.setSessionManager(&sessions);
    router.registerService("registration", &registrationService);

    Request request;
    request.module = "registration";
    request.action = "create";

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("未登录") || response.message.contains("失效"));
}

void AuthRouterTests::registrationHistoryRequiresLogin()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService registrationService;
    router.setSessionManager(&sessions);
    router.registerService("registration", &registrationService);

    Request request;
    request.module = "registration";
    request.action = "history";

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("未登录") || response.message.contains("失效"));
}

void AuthRouterTests::authorizationHeaderBearerTokenAuthenticatesRequest()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService registrationService;
    router.setSessionManager(&sessions);
    router.registerService("registration", &registrationService);
    const auto session = sessions.createSession("3",
                                                "reg01",
                                                "挂号员一号",
                                                "REGISTRAR",
                                                "挂号员",
                                                QStringList{"registration:history"});

    Request request;
    request.module = "registration";
    request.action = "history";
    request.headers["Authorization"] = "Bearer " + session.token;

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("operatorRoleCode").toString(), QString("REGISTRAR"));
    QCOMPARE(response.data.value("operatorName").toString(), QString("挂号员一号"));
}

void AuthRouterTests::patientTokenCanCreateRegistrationWithPatientIdentity()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService registrationService;
    router.setSessionManager(&sessions);
    router.registerService("registration", &registrationService);
    const auto session = sessions.createPatientSession("101", "13800000001", "测试患者");

    Request request;
    request.module = "registration";
    request.action = "create";
    request.headers["Authorization"] = "Bearer " + session.token;

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("patientId").toString(), QString("101"));
    QCOMPARE(response.data.value("currentUserType").toString(), QString("PATIENT"));
}

void AuthRouterTests::patientTokenCanReadOwnHistoryWithPatientIdentity()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService registrationService;
    router.setSessionManager(&sessions);
    router.registerService("registration", &registrationService);
    const auto session = sessions.createPatientSession("102", "13800000002", "历史患者");

    Request request;
    request.module = "registration";
    request.action = "history";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("patientId").toString(), QString("102"));
    QCOMPARE(response.data.value("currentUserType").toString(), QString("PATIENT"));
}

void AuthRouterTests::patientTokenCanConfirmOwnPaymentWithPatientIdentity()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService billingService;
    router.setSessionManager(&sessions);
    router.registerService("billing", &billingService);
    const auto session = sessions.createPatientSession("103", "13800000003", "支付患者");

    Request request;
    request.module = "billing";
    request.action = "pay";
    request.token = session.token;
    request.payload["账单号"] = "B202606150001";
    request.payload["amount"] = 25.0;

    const Response response = router.route(request);

    QVERIFY2(response.success, qPrintable(response.message));
    QCOMPARE(response.data.value("currentUserType").toString(), QString("PATIENT"));
}

void AuthRouterTests::auditDetailsAreNotReturnedToClient()
{
    SessionManager sessions;
    RequestRouter router;
    AuditService service;
    router.setSessionManager(&sessions);
    router.registerService("patientRecord", &service);
    const auto session = sessions.createSession("1",
                                                "admin",
                                                "系统管理员",
                                                "ADMIN",
                                                "系统管理员",
                                                QStringList{"patientRecord:update"});

    Request request;
    request.module = "patientRecord";
    request.action = "update";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("visible").toString(), QString("ok"));
    QVERIFY(!response.data.contains("__auditDetails"));
}

void AuthRouterTests::directorDemoAccountLogsInWithDefaultPassword()
{
    hospital::server::DatabaseManager database;
    SessionManager sessions;
    hospital::server::AuthService authService(&database, &sessions);

    Request request;
    request.module = "auth";
    request.action = "login";
    request.payload["username"] = "director01";
    request.payload["password"] = "123456";

    const Response response = authService.handle(request);

    QVERIFY(response.success);
    QCOMPARE(response.data.value("roleCode").toString(), QString("DIRECTOR"));
    QCOMPARE(response.data.value("username").toString(), QString("director01"));
    QVERIFY(response.data.contains("permissions"));
    QVERIFY(response.data.value("permissions").isArray());
    const QJsonArray permissions = response.data.value("permissions").toArray();
    QVERIFY(permissions.contains("schedule:list"));
    QVERIFY(response.data.contains("dataScope"));
    QVERIFY(!response.data.value("dataScope").toString().trimmed().isEmpty());
}

void AuthRouterTests::adminWithoutExplicitDoctorCreatePermissionIsRejected()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService doctorService;
    router.setSessionManager(&sessions);
    router.registerService("doctor", &doctorService);
    const auto session = sessions.createSession("1",
                                                "admin",
                                                "系统管理员",
                                                "ADMIN",
                                                "系统管理员",
                                                QStringList{"permissionAdmin:users", "doctor:list"});

    Request request;
    request.module = "doctor";
    request.action = "create";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("权限不足"));
}

void AuthRouterTests::adminWithEmptyPermissionListCannotFallBackToDoctorCreate()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService doctorService;
    router.setSessionManager(&sessions);
    router.registerService("doctor", &doctorService);
    const auto session = sessions.createSession("1",
                                                "admin",
                                                "系统管理员",
                                                "ADMIN",
                                                "系统管理员",
                                                QStringList{});

    Request request;
    request.module = "doctor";
    request.action = "create";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY(!response.success);
    QVERIFY(response.message.contains("权限不足"));
}

void AuthRouterTests::adminCanAlwaysReadOperationLogForRecovery()
{
    SessionManager sessions;
    RequestRouter router;
    EchoService operationLogService;
    router.setSessionManager(&sessions);
    router.registerService("operationLog", &operationLogService);
    const auto session = sessions.createSession("1",
                                                "admin",
                                                "系统管理员",
                                                "ADMIN",
                                                "系统管理员",
                                                QStringList{});

    Request request;
    request.module = "operationLog";
    request.action = "list";
    request.token = session.token;

    const Response response = router.route(request);

    QVERIFY2(response.success, qPrintable(response.message));
}

QTEST_MAIN(AuthRouterTests)
#include "auth_router_tests.moc"
