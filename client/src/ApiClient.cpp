#include "client/ApiClient.h"

#include <QDebug>
#include <QJsonArray>

namespace hospital::client {

namespace {
constexpr qsizetype kMaxFrameBytes = 1024 * 1024;
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &ApiClient::connected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ApiClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket.errorString());
    });
}

void ApiClient::connectToServer(const QString& host, quint16 port)
{
    m_serverHost = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_serverPort = port;
    if (m_socket.state() == QAbstractSocket::ConnectedState
        || m_socket.state() == QAbstractSocket::ConnectingState) {
        return;
    }
    m_socket.connectToHost(host, port);
}

bool ApiClient::send(const common::Request& request)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred("服务端未连接，请先启动 hospital_server。");
        return false;
    }

    common::Request copy = request;
    if (copy.token.isEmpty()) {
        copy.token = m_token;
    }
    if (!m_token.isEmpty() && copy.headers.value("Authorization").toString().trimmed().isEmpty()) {
        copy.headers["Authorization"] = "Bearer " + m_token;
    }
    if (!m_userId.isEmpty() && m_roleCode != "PATIENT") {
        copy.payload["__operatorUserId"] = m_userId;
        copy.payload["__operatorName"] = m_realName.isEmpty() ? m_username : m_realName;
        copy.payload["__operatorRoleCode"] = m_roleCode;
        copy.payload["__operatorRole"] = m_roleName;
    }
    const QByteArray frame = common::Protocol::encodeRequest(copy);
    const qint64 written = m_socket.write(frame);
    if (written != frame.size()) {
        emit errorOccurred(QStringLiteral("请求写入失败：%1").arg(m_socket.errorString()));
        return false;
    }

    if (!m_socket.flush()) {
        qWarning().noquote()
            << "ApiClient::send flush returned false"
            << "module=" + copy.module
            << "action=" + copy.action
            << "error=" + m_socket.errorString();
    }
    return true;
}

bool ApiClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool ApiClient::isLoggedIn() const
{
    return !m_token.trimmed().isEmpty();
}

QString ApiClient::token() const
{
    return m_token;
}

void ApiClient::setToken(const QString& token)
{
    m_token = token;
}

QString ApiClient::userId() const
{
    return m_userId;
}

QString ApiClient::username() const
{
    return m_username;
}

QString ApiClient::realName() const
{
    return m_realName;
}

QString ApiClient::roleCode() const
{
    return m_roleCode;
}

QString ApiClient::roleName() const
{
    return m_roleName;
}

QString ApiClient::doctorId() const
{
    return m_doctorId;
}

bool ApiClient::hasPermission(const QString& permission) const
{
    const QString normalized = permission.trimmed();
    if (normalized.isEmpty()) {
        return true;
    }

    const int separator = normalized.indexOf(':');
    const QString moduleWildcard = separator > 0 ? normalized.left(separator) + ":*" : QString();
    return m_permissions.contains(normalized)
        || (!moduleWildcard.isEmpty() && m_permissions.contains(moduleWildcard))
        || m_permissions.contains("*:*");
}

void ApiClient::setSession(const QJsonObject& data)
{
    m_token = data.value("token").toString();
    m_userId = data.value("userId").toVariant().toString();
    m_username = data.value("username").toString();
    m_realName = data.value("realName").toString();
    m_roleCode = data.value("roleCode").toString();
    m_roleName = data.value("roleName").toString();
    m_doctorId = data.value("doctorId").toVariant().toString();
    m_permissions.clear();
    const QJsonArray permissions = data.value("permissions").toArray();
    for (const auto& permission : permissions) {
        const QString code = permission.toString().trimmed();
        if (!code.isEmpty()) {
            m_permissions.append(code);
        }
    }
    m_patientUserId.clear();
    m_patientId.clear();
    m_patientName.clear();
    m_patientPhone.clear();
    m_patientIdCard.clear();
}

void ApiClient::setPatientSession(const QJsonObject& data)
{
    m_token = data.value("token").toString();
    m_userId.clear();
    m_username = data.value("username").toString();
    m_realName = data.value("name").toString(data.value("realName").toString());
    m_roleCode = "PATIENT";
    m_roleName = "患者";
    m_doctorId.clear();
    m_permissions = {"registration:create", "registration:history",
                     "billing:createPaymentQr", "billing:checkPayStatus",
                     "billing:medicalInsurancePay", "billing:pay"};
    m_patientUserId = data.value("patientUserId").toVariant().toString();
    if (m_patientUserId.isEmpty()) {
        m_patientUserId = data.value("userId").toVariant().toString();
    }
    m_patientId = data.value("patientId").toVariant().toString();
    m_patientName = data.value("name").toString(data.value("realName").toString());
    m_patientPhone = data.value("phone").toString();
    m_patientIdCard = data.value("idCard").toString();
}

bool ApiClient::isPatientLoggedIn() const
{
    return !m_token.trimmed().isEmpty() && !m_patientUserId.trimmed().isEmpty();
}

QString ApiClient::patientUserId() const
{
    return m_patientUserId;
}

QString ApiClient::patientId() const
{
    return m_patientId;
}

QString ApiClient::patientName() const
{
    return m_patientName;
}

QString ApiClient::patientPhone() const
{
    return m_patientPhone;
}

QString ApiClient::patientIdCard() const
{
    return m_patientIdCard;
}

QString ApiClient::serverHost() const
{
    return m_serverHost;
}

quint16 ApiClient::serverPort() const
{
    return m_serverPort;
}

void ApiClient::onReadyRead()
{
    m_buffer += m_socket.readAll();
    if (m_buffer.size() > kMaxFrameBytes) {
        m_buffer.clear();
        emit errorOccurred(QStringLiteral("服务端响应过大，已丢弃。"));
        m_socket.disconnectFromHost();
        return;
    }

    while (true) {
        const int lineEnd = m_buffer.indexOf('\n');
        if (lineEnd < 0) {
            break;
        }

        const QByteArray frame = m_buffer.left(lineEnd);
        m_buffer.remove(0, lineEnd + 1);

        common::Response response;
        QString decodeError;
        if (!common::Protocol::tryDecodeResponse(frame, &response, &decodeError)) {
            emit errorOccurred(QStringLiteral("服务端响应格式错误：%1").arg(decodeError));
            continue;
        }

        emit responseReceived(response);
    }
}

} // namespace hospital::client
