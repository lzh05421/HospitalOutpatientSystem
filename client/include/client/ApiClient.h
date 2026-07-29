#pragma once

#include "common/Protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <QStringList>

namespace hospital { namespace client {

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port);
    bool send(const common::Request& request);
    bool isConnected() const;
    bool isLoggedIn() const;
    QString token() const;
    void setToken(const QString& token);
    QString userId() const;
    QString username() const;
    QString realName() const;
    QString roleCode() const;
    QString roleName() const;
    QString doctorId() const;
    bool hasPermission(const QString& permission) const;
    void setSession(const QJsonObject& data);
    void setPatientSession(const QJsonObject& data);
    bool isPatientLoggedIn() const;
    QString patientUserId() const;
    QString patientId() const;
    QString patientName() const;
    QString patientPhone() const;
    QString patientIdCard() const;
    QString serverHost() const;
    quint16 serverPort() const;

signals:
    void connected();
    void errorOccurred(const QString& message);
    void responseReceived(const common::Response& response);

private slots:
    void onReadyRead();

private:
    QTcpSocket m_socket;
    QByteArray m_buffer;
    QString m_token;
    QString m_userId;
    QString m_username;
    QString m_realName;
    QString m_roleCode;
    QString m_roleName;
    QString m_doctorId;
    QStringList m_permissions;
    QString m_patientUserId;
    QString m_patientId;
    QString m_patientName;
    QString m_patientPhone;
    QString m_patientIdCard;
    QString m_serverHost = "127.0.0.1";
    quint16 m_serverPort = 8899;
};

}} // namespace hospital::client
