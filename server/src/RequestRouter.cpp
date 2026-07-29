#include "server/RequestRouter.h"

#include "server/AuthorizationService.h"
#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace hospital::server {

namespace {

QString bearerToken(const common::Request& request)
{
    const QString authorization = request.headers.value("Authorization").toString().trimmed();
    if (authorization.startsWith("Bearer ", Qt::CaseInsensitive)) {
        return authorization.mid(7).trimmed();
    }
    return request.token;
}

} // namespace

void RequestRouter::registerService(const QString& module, ModuleService* service)
{
    m_services.insert(module, service);
}

void RequestRouter::setDatabase(DatabaseManager* database)
{
    m_database = database;
}

void RequestRouter::setSessionManager(SessionManager* sessions)
{
    m_sessions = sessions;
}

common::Response RequestRouter::route(const common::Request& request) const
{
    const auto auth = authorize(request);
    if (!auth.success) {
        common::Response response{false, auth.message, {}};
        response.data["module"] = request.module;
        response.data["action"] = request.action;
        return response;
    }

    auto routedRequest = request;
    if (auth.authenticated && auth.session.userType == "PATIENT") {
        routedRequest.payload["__patientUserId"] = auth.session.patientUserId;
        routedRequest.payload["__patientId"] = auth.session.patientId;
        routedRequest.payload["__patientName"] = auth.session.patientName;
        routedRequest.payload["__patientPhone"] = auth.session.patientPhone;
        routedRequest.payload["__patientIdCard"] = auth.session.patientIdCard;
        routedRequest.payload["__currentUserType"] = "PATIENT";
    } else if (auth.authenticated) {
        routedRequest.payload["__operatorUserId"] = auth.session.userId;
        routedRequest.payload["__operatorName"] = auth.session.realName.isEmpty()
            ? auth.session.username
            : auth.session.realName;
        routedRequest.payload["__operatorRoleCode"] = auth.session.roleCode;
        routedRequest.payload["__operatorRole"] = auth.session.roleName;
        QJsonArray permissions;
        for (const auto& permission : auth.session.permissions) {
            permissions.append(permission);
        }
        routedRequest.payload["__permissions"] = permissions;
        routedRequest.payload["__dataScope"] = auth.session.dataScope;
        routedRequest.payload["__dataScopeCode"] = auth.session.dataScope;
        QJsonArray deptIds;
        for (const auto& deptId : auth.session.departmentIds) {
            deptIds.append(deptId);
        }
        routedRequest.payload["__deptIds"] = deptIds;
        routedRequest.payload["__departmentScopeIds"] = deptIds;
        routedRequest.payload["__primaryDeptId"] = auth.session.primaryDeptId;
        routedRequest.payload["__doctorId"] = auth.session.doctorId;
    } else {
        const QStringList internalKeys = {
            "__operatorUserId", "__operatorName", "__operatorRoleCode", "__operatorRole",
            "__permissions", "__dataScope", "__dataScopeCode", "__deptIds",
            "__departmentScopeIds", "__primaryDeptId", "__doctorId"
        };
        for (const QString& key : internalKeys) {
            routedRequest.payload.remove(key);
        }
        routedRequest.payload.remove("__patientId");
        routedRequest.payload.remove("__patientUserId");
        routedRequest.payload.remove("__patientName");
        routedRequest.payload.remove("__patientPhone");
        routedRequest.payload.remove("__patientIdCard");
        routedRequest.payload.remove("__currentUserType");
    }

    auto* service = m_services.value(routedRequest.module, nullptr);
    if (!service) {
        return {false, QString("Unknown module: %1").arg(routedRequest.module), {}};
    }

    auto response = service->handle(routedRequest);
    response.data["module"] = routedRequest.module;
    response.data["action"] = routedRequest.action;
    const qint64 operationLogId = writeOperationLog(routedRequest, response);
    writeAuditDetails(operationLogId, routedRequest, response);
    response.data = publicResponseData(response.data);
    return response;
}

RequestRouter::AuthorizationResult RequestRouter::authorize(const common::Request& request) const
{
    if (request.module == "auth" && (request.action == "login"
                                      || request.action == "patientLogin"
                                      || request.action == "patientRegister")) {
        return {true, "OK", false, {}};
    }

    const bool publicAction = (request.module == "schedule" && request.action == "list")
        || (request.module == "billing" && request.action == "insuranceCallback")
        || (request.module == "billing" && request.action == "mockPay");
    if (publicAction) {
        return {true, "OK", false, {}};
    }

    if (!m_sessions) {
        return {false, "服务端会话管理未初始化，请检查 server 配置。", false, {}};
    }

    Session session;
    if (!m_sessions->findSession(bearerToken(request), &session)) {
        return {false, "未登录或会话已失效，请重新登录。", false, {}};
    }

    if (AuthorizationService::canAccess(session, request.module, request.action)) {
        return {true, "OK", true, session};
    }

    return {false,
            QString("权限不足：%1 不能执行 %2/%3。").arg(session.roleCode, request.module, request.action),
            true,
            session};
}

bool RequestRouter::isAllowed(const QString& roleCode, const QString& module, const QString& action) const
{
    Session session;
    session.roleCode = roleCode;
    return AuthorizationService::canAccess(session, module, action);
}

qint64 RequestRouter::writeOperationLog(const common::Request& request, const common::Response& response) const
{
    if (!response.success || request.module == "auth") {
        return 0;
    }

    const QStringList writeActions = {
        "create", "update", "delete", "reset", "save", "saveWaiting", "rulesSaveAll", "start", "inbound",
        "pay", "medicalInsurancePay", "insuranceCallback", "refund", "review", "reject", "dispense", "return", "call", "complete",
        "createUser", "resetPassword", "toggleUser", "saveRolePermissions"
    };
    if (!writeActions.contains(request.action)) {
        return 0;
    }

    const QString operatorName = request.payload.value("__operatorName").toString("系统");
    const QString content = QString("%1/%2 %3")
        .arg(request.module, request.action,
             QString::fromUtf8(QJsonDocument(request.payload).toJson(QJsonDocument::Compact)));

    if (!m_database || !m_database->isEnabled()) {
        DemoRepository::instance().appendOperationLog(operatorName, request.module, request.action, content);
        return 0;
    }

    if (!m_database->ensureOpen()) {
        return 0;
    }

    const QString userId = request.payload.value("__operatorUserId").toVariant().toString();
    QSqlQuery query(m_database->database());
    query.prepare("INSERT INTO operation_logs (user_id, module, action, content) "
                  "VALUES (:user_id, :module, :action, :content)");
    query.bindValue(":user_id", userId.isEmpty() ? QVariant() : QVariant(userId.toLongLong()));
    query.bindValue(":module", request.module.left(64));
    query.bindValue(":action", request.action.left(64));
    query.bindValue(":content", content.left(500));
    if (!query.exec()) {
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

void RequestRouter::writeAuditDetails(qint64 operationLogId, const common::Request& request, const common::Response& response) const
{
    const QJsonArray details = response.data.value("__auditDetails").toArray();
    if (details.isEmpty() || !m_database || !m_database->isEnabled()) {
        return;
    }

    if (!m_database->ensureOpen()) {
        return;
    }

    const QString userId = request.payload.value("__operatorUserId").toVariant().toString();
    const QVariant userIdValue = userId.isEmpty() ? QVariant() : QVariant(userId.toLongLong());
    for (const auto& item : details) {
        const auto detail = item.toObject();
        QSqlQuery query(m_database->database());
        query.prepare("INSERT INTO audit_log_details "
                      "(operation_log_id, user_id, module, action, business_key, field_name, old_value, new_value, change_reason) "
                      "VALUES (:operation_log_id, :user_id, :module, :action, :business_key, :field_name, :old_value, :new_value, :change_reason)");
        query.bindValue(":operation_log_id", operationLogId > 0 ? QVariant(operationLogId) : QVariant());
        query.bindValue(":user_id", userIdValue);
        query.bindValue(":module", request.module);
        query.bindValue(":action", request.action);
        query.bindValue(":business_key", detail.value("businessKey").toString().left(128));
        query.bindValue(":field_name", detail.value("fieldName").toString().left(64));
        query.bindValue(":old_value", detail.value("oldValue").toVariant().toString().left(1000));
        query.bindValue(":new_value", detail.value("newValue").toVariant().toString().left(1000));
        query.bindValue(":change_reason", detail.value("changeReason").toString().left(255));
        query.exec();
    }
}

QJsonObject RequestRouter::publicResponseData(QJsonObject data) const
{
    data.remove("__auditDetails");
    return data;
}

} // namespace hospital::server
