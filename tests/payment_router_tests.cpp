#include "server/DatabaseManager.h"
#include "server/RequestRouter.h"
#include "server/SessionManager.h"
#include "server/modules/ModuleServices.h"

#include <QtTest/QtTest>

using hospital::common::Request;
using hospital::common::Response;
using hospital::server::BillingService;
using hospital::server::DatabaseManager;
using hospital::server::RequestRouter;
using hospital::server::SessionManager;

class PaymentRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void patientTokenCanCreatePaymentQrWithoutTimeoutInDemoMode();
    void patientTokenCanRequestMedicalInsuranceWithoutTimeoutInDemoMode();
};

void PaymentRouterTests::patientTokenCanCreatePaymentQrWithoutTimeoutInDemoMode()
{
    DatabaseManager database;
    SessionManager sessions;
    RequestRouter router;
    BillingService billing(&database);
    router.setSessionManager(&sessions);
    router.setDatabase(&database);
    router.registerService("billing", &billing);

    const auto session = sessions.createPatientSession("101", "13800000001", "测试患者", "13800000001", "110101199001011234", "201");

    Request request;
    request.module = "billing";
    request.action = "createPaymentQr";
    request.headers["Authorization"] = "Bearer " + session.token;
    request.payload["billNo"] = "BTEST001";
    request.payload["amount"] = 25.0;
    request.payload["OrderID"] = "BTEST001";
    request.payload["Amount"] = 25.0;
    request.payload["NotifyURL"] = "hospital://payment/notify";

    const Response response = router.route(request);

    QVERIFY2(response.success, qPrintable(response.message));
    QCOMPARE(response.data.value("module").toString(), QString("billing"));
    QCOMPARE(response.data.value("action").toString(), QString("createPaymentQr"));
    QCOMPARE(response.data.value("billNo").toString(), QString("BTEST001"));
    QVERIFY(!response.data.value("qrPayload").toString().isEmpty());
    QVERIFY(!response.data.value("qrImageBase64").toString().isEmpty());
}

void PaymentRouterTests::patientTokenCanRequestMedicalInsuranceWithoutTimeoutInDemoMode()
{
    DatabaseManager database;
    SessionManager sessions;
    RequestRouter router;
    BillingService billing(&database);
    router.setSessionManager(&sessions);
    router.setDatabase(&database);
    router.registerService("billing", &billing);

    const auto session = sessions.createPatientSession("101", "13800000001", "测试患者", "13800000001", "110101199001011234", "201");

    Request request;
    request.module = "billing";
    request.action = "medicalInsurancePay";
    request.headers["Authorization"] = "Bearer " + session.token;
    request.payload["billNo"] = "BTEST002";
    request.payload["账单号"] = "BTEST002";
    request.payload["amount"] = 25.0;

    const Response response = router.route(request);

    QVERIFY2(response.success, qPrintable(response.message));
    QCOMPARE(response.data.value("module").toString(), QString("billing"));
    QCOMPARE(response.data.value("action").toString(), QString("medicalInsurancePay"));
    QCOMPARE(response.data.value("paymentStatus").toString(), QString("PAID"));
    QCOMPARE(response.data.value("registrationStatus").toString(), QString("WAITING"));
    QCOMPARE(response.data.value("simulationMode").toString(), QString("LOCAL_REGISTRATION_INSURANCE"));
    QVERIFY(response.data.value("h5PaymentUrl").toString().isEmpty());
}

QTEST_MAIN(PaymentRouterTests)
#include "payment_router_tests.moc"
