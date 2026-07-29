#include <common/Protocol.h>

#include <QtTest/QtTest>

class ProtocolTests : public QObject
{
    Q_OBJECT

private slots:
    void requestRoundTripPreservesFields();
    void tryDecodeRequestRejectsMalformedJson();
    void tryDecodeRequestRejectsMissingModuleOrAction();
};

void ProtocolTests::requestRoundTripPreservesFields()
{
    hospital::common::Request request;
    request.module = "auth";
    request.action = "login";
    request.token = "demo-token";
    request.payload["username"] = "admin";

    const hospital::common::Request decoded = hospital::common::Protocol::decodeRequest(
        hospital::common::Protocol::encodeRequest(request));

    QCOMPARE(decoded.module, request.module);
    QCOMPARE(decoded.action, request.action);
    QCOMPARE(decoded.token, request.token);
    QCOMPARE(decoded.payload.value("username").toString(), QString("admin"));
}

void ProtocolTests::tryDecodeRequestRejectsMalformedJson()
{
    hospital::common::Request request;
    QString error;

    const bool ok = hospital::common::Protocol::tryDecodeRequest("{not json}\n", &request, &error);

    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
}

void ProtocolTests::tryDecodeRequestRejectsMissingModuleOrAction()
{
    hospital::common::Request request;
    QString error;

    const bool ok = hospital::common::Protocol::tryDecodeRequest("{\"module\":\"patient\",\"payload\":{}}\n", &request, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains("module") || error.contains("action"));
}

QTEST_MAIN(ProtocolTests)
#include "protocol_tests.moc"
