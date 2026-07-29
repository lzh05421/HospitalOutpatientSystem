#include "server/ClientConnection.h"

#include "common/Protocol.h"
#include "server/RequestRouter.h"

#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace hospital::server {

namespace {
constexpr qsizetype kMaxFrameBytes = 1024 * 1024;
constexpr int kScanPaymentStatusTtlSeconds = 300;

struct ScanPaymentRecord
{
    QString tokenHash;
    QDateTime expiresAt;
};

QHash<QString, ScanPaymentRecord>& scanPaymentStatus()
{
    static QHash<QString, ScanPaymentRecord> status;
    return status;
}

QString scanTokenHash(const QString& token)
{
    return QString::fromLatin1(QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void rememberScannedPayment(const QString& billNo, const QString& token)
{
    scanPaymentStatus().insert(billNo, {scanTokenHash(token), QDateTime::currentDateTimeUtc().addSecs(kScanPaymentStatusTtlSeconds)});
}

bool isScannedPaymentPaid(const QString& billNo, const QString& token)
{
    auto& status = scanPaymentStatus();
    const auto it = status.find(billNo);
    if (it == status.end()) {
        return false;
    }
    if (it->expiresAt < QDateTime::currentDateTimeUtc()) {
        status.erase(it);
        return false;
    }
    return it->tokenHash == scanTokenHash(token);
}

QByteArray httpResponse(const QByteArray& status, const QByteArray& body)
{
    return "HTTP/1.1 " + status + "\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

QByteArray jsonResponse(const QByteArray& status, const QJsonObject& data)
{
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    return "HTTP/1.1 " + status + "\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

QByteArray htmlPage(const QString& title, const QString& message)
{
    return QString("<!doctype html><html><head><meta charset=\"utf-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                   "<title>%1</title></head><body>"
                   "<h1>%1</h1><p>%2</p></body></html>")
        .arg(title.toHtmlEscaped(), message.toHtmlEscaped())
        .toUtf8();
}

QByteArray paymentResultPage(const QString& title, const QString& billNo, const QString& message)
{
    const QString billLine = billNo.trimmed().isEmpty()
        ? QString()
        : QString("<p>账单号：%1</p>").arg(billNo.toHtmlEscaped());
    return QString("<!doctype html><html><head><meta charset=\"utf-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                   "<title>%1</title></head><body>"
                   "<h1>%1</h1>%2<p>%3</p></body></html>")
        .arg(title.toHtmlEscaped(), billLine, message.toHtmlEscaped())
        .toUtf8();
}
}

ClientConnection::ClientConnection(QTcpSocket* socket, RequestRouter* router, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
    , m_router(router)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
}

void ClientConnection::onReadyRead()
{
    m_buffer += m_socket->readAll();
    if (m_buffer.size() > kMaxFrameBytes) {
        const common::Response response{false, QStringLiteral("请求过大，已断开连接。"), {}};
        m_socket->write(common::Protocol::encodeResponse(response));
        m_socket->disconnectFromHost();
        m_buffer.clear();
        return;
    }

    if (m_buffer.startsWith("GET ") || m_buffer.startsWith("POST ") || m_buffer.startsWith("HEAD ")) {
        if (!m_buffer.contains("\r\n\r\n")) {
            return;
        }
        handleHttpMockPay();
        return;
    }

    while (true) {
        const int lineEnd = m_buffer.indexOf('\n');
        if (lineEnd < 0) {
            break;
        }

        const QByteArray frame = m_buffer.left(lineEnd);
        m_buffer.remove(0, lineEnd + 1);

        common::Request request;
        QString decodeError;
        if (!common::Protocol::tryDecodeRequest(frame, &request, &decodeError)) {
            const common::Response response{false, QStringLiteral("请求格式错误：%1").arg(decodeError), {}};
            m_socket->write(common::Protocol::encodeResponse(response));
            continue;
        }

        const auto response = m_router->route(request);
        m_socket->write(common::Protocol::encodeResponse(response));
        m_socket->flush();
    }
}

void ClientConnection::handleHttpMockPay()
{
    const QList<QByteArray> lines = m_buffer.split('\n');
    const QByteArray requestLine = lines.isEmpty() ? QByteArray() : lines.first().trimmed();
    m_buffer.clear();

    const QList<QByteArray> parts = requestLine.split(' ');
    const QUrl url = parts.size() >= 2 ? QUrl(QString::fromUtf8(parts.at(1))) : QUrl();
    if (url.path() != "/mock-pay" && url.path() != "/p" && url.path() != "/p-status") {
        const QByteArray body = htmlPage("支付链接无效", "请重新打开支付二维码。");
        m_socket->write(httpResponse("404 Not Found", body));
        m_socket->disconnectFromHost();
        return;
    }

    const QUrlQuery query(url);
    const QString billNo = query.queryItemValue("b").trimmed().isEmpty()
        ? query.queryItemValue("billNo").trimmed()
        : query.queryItemValue("b").trimmed();
    const QString token = query.queryItemValue("t").trimmed().isEmpty()
        ? query.queryItemValue("token").trimmed()
        : query.queryItemValue("t").trimmed();

    if (url.path() == "/p-status") {
        const bool paid = !billNo.isEmpty() && !token.isEmpty() && isScannedPaymentPaid(billNo, token);
        QJsonObject data;
        data["billNo"] = billNo;
        data["paymentStatus"] = paid ? "PAID" : "UNPAID";
        data["scanPaymentStatus"] = true;
        m_socket->write(jsonResponse("200 OK", data));
        m_socket->flush();
        m_socket->disconnectFromHost();
        return;
    }

    if (billNo.isEmpty() || token.isEmpty()) {
        qWarning().noquote()
            << "ScanPaymentRequest"
            << "success=false"
            << "reason=missing_bill_or_token"
            << "billNo=" + billNo;
        const QByteArray body = paymentResultPage("支付失败", billNo, "支付链接缺少账单号或支付凭证，请从新增挂号后的待支付窗口重新生成二维码。");
        m_socket->write(httpResponse("400 Bad Request", body));
        m_socket->disconnectFromHost();
        return;
    }

    common::Request request;
    request.module = "billing";
    request.action = "mockPay";
    request.payload["billNo"] = billNo;
    request.payload["账单号"] = billNo;
    request.payload["paymentToken"] = token;
    request.payload["__mockPay"] = true;
    request.payload["支付方式"] = "微信/支付宝扫码";

    qInfo().noquote()
        << "ScanPaymentRequest"
        << "billNo=" + billNo
        << "tokenPresent=true";
    const auto response = m_router->route(request);
    if (response.success) {
        rememberScannedPayment(billNo, token);
    }
    qInfo().noquote()
        << "ScanPaymentResponse"
        << "billNo=" + billNo
        << "success=" + QString(response.success ? "true" : "false")
        << "message=" + response.message;
    const QByteArray body = response.success
        ? paymentResultPage("支付成功", billNo, "系统已收到手机扫码支付，挂号已完成，请返回电脑查看结果。")
        : paymentResultPage("支付失败", billNo, response.message);
    m_socket->write(httpResponse(response.success ? "200 OK" : "409 Conflict", body));
    m_socket->flush();
    m_socket->disconnectFromHost();
}

} // namespace hospital::server
