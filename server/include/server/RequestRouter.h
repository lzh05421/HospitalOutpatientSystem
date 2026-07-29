#pragma once

#include "common/Protocol.h"
#include "server/SessionManager.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace hospital { namespace server {

class DatabaseManager;

class ModuleService
{
public:
    virtual ~ModuleService() = default;
    virtual common::Response handle(const common::Request& request) = 0;
};

class RequestRouter
{
public:
    void registerService(const QString& module, ModuleService* service);
    void setDatabase(DatabaseManager* database);
    void setSessionManager(SessionManager* sessions);
    common::Response route(const common::Request& request) const;

private:
    struct AuthorizationResult
    {
        bool success = false;
        QString message;
        bool authenticated = false;
        Session session;
    };

    AuthorizationResult authorize(const common::Request& request) const;
    bool isAllowed(const QString& roleCode, const QString& module, const QString& action) const;
    qint64 writeOperationLog(const common::Request& request, const common::Response& response) const;
    void writeAuditDetails(qint64 operationLogId, const common::Request& request, const common::Response& response) const;
    QJsonObject publicResponseData(QJsonObject data) const;

    QHash<QString, ModuleService*> m_services;
    DatabaseManager* m_database = nullptr;
    SessionManager* m_sessions = nullptr;
};

}} // namespace hospital::server
