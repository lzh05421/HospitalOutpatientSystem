#include "server/HospitalServer.h"

#include "server/ClientConnection.h"

#include <QHostAddress>

namespace hospital::server {

HospitalServer::HospitalServer(RequestRouter* router, QObject* parent)
    : QObject(parent)
    , m_router(router)
{
    connect(&m_server, &QTcpServer::newConnection, this, &HospitalServer::onNewConnection);
}

bool HospitalServer::listen(const AppConfig& config)
{
    return m_server.listen(QHostAddress(config.serverHost), config.serverPort);
}

void HospitalServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        new ClientConnection(m_server.nextPendingConnection(), m_router, this);
    }
}

} // namespace hospital::server
