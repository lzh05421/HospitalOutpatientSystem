#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace hospital::server {
namespace {

common::Response createDepartment(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString name = payload.value("科室名称").toString().trimmed();
    if (name.isEmpty()) {
        return {false, "科室名称不能为空。", {}};
    }

    QString code = payload.value("科室编码").toString().trimmed();
    if (code.isEmpty()) {
        code = "DEP" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    }

    QSqlQuery query(database->database());
    query.prepare("INSERT INTO departments (dept_code, dept_name, location, status) "
                  "VALUES (:code, :name, :location, 1) "
                  "ON DUPLICATE KEY UPDATE dept_name = VALUES(dept_name), "
                  "location = VALUES(location), status = 1");
    query.bindValue(":code", code);
    query.bindValue(":name", name);
    query.bindValue(":location", payload.value("位置").toString().trimmed());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    return {true, "科室已保存。", {}};
}

common::Response updateDepartment(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString oldCode = payload.value("原科室编码").toString(payload.value("科室编码").toString()).trimmed();
    const QString newCode = payload.value("科室编码").toString().trimmed();
    const QString name = payload.value("科室名称").toString().trimmed();
    if (oldCode.isEmpty() || newCode.isEmpty() || name.isEmpty()) {
        return {false, "科室编码和科室名称不能为空。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE departments SET dept_code = :new_code, dept_name = :name, "
                  "location = :location, status = :status WHERE dept_code = :old_code");
    query.bindValue(":new_code", newCode);
    query.bindValue(":name", name);
    query.bindValue(":location", payload.value("位置").toString().trimmed());
    query.bindValue(":status", payload.value("状态").toVariant().toInt());
    query.bindValue(":old_code", oldCode);
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    return {true, "科室信息已修改。", {}};
}

common::Response disableDepartment(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE departments SET status = 0 WHERE dept_code = :code");
    query.bindValue(":code", payload.value("科室编码").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    return {true, "科室已停用。", {}};
}

} // namespace

DepartmentService::DepartmentService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response DepartmentService::handle(const common::Request& request)
{
    if (request.action == "create") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().saveDepartment(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return createDepartment(m_database, request.payload);
    }
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updateDepartment(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateDepartment(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().disableDepartment(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return disableDepartment(m_database, request.payload);
    }
    if (request.action != "list") {
        return {false, "Unsupported department action", {}};
    }

    return SqlJson::selectRows(m_database,
        "SELECT dept_code AS '科室编码', dept_name AS '科室名称', "
        "location AS '位置', status AS '状态' "
        "FROM departments WHERE status = 1 ORDER BY dept_code LIMIT 300",
        {}, "departments");
}

} // namespace hospital::server
