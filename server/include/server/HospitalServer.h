#pragma once

#include "server/AppConfig.h"

#include <QObject>
#include <QTcpServer>

namespace hospital { namespace server {

class RequestRouter;

class HospitalServer : public QObject
{
    Q_OBJECT

public:
    explicit HospitalServer(RequestRouter* router, QObject* parent = nullptr);
    bool listen(const AppConfig& config);

private slots:
    void onNewConnection();

private:
    QTcpServer m_server;
    RequestRouter* m_router = nullptr;
};

}} // namespace hospital::server
