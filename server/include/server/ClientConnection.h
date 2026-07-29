#pragma once

#include <QObject>
#include <QTcpSocket>

namespace hospital { namespace server {

class RequestRouter;

class ClientConnection : public QObject
{
    Q_OBJECT

public:
    explicit ClientConnection(QTcpSocket* socket, RequestRouter* router, QObject* parent = nullptr);

private slots:
    void onReadyRead();

private:
    void handleHttpMockPay();

    QTcpSocket* m_socket = nullptr;
    RequestRouter* m_router = nullptr;
    QByteArray m_buffer;
};

}} // namespace hospital::server
