#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSet>
#include <QVariant>

namespace hospital::server {
namespace {

QString sha256(const QString& text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QJsonObject userRow(const QSqlQuery& query)
{
    QJsonObject row;
    row["用户ID"] = QString::number(query.value(0).toLongLong());
    row["账号"] = query.value(1).toString();
    row["姓名"] = query.value(2).toString();
    row["电话"] = query.value(3).toString();
    row["角色编码"] = query.value(4).toString();
    row["角色名称"] = query.value(5).toString();
    row["状态"] = query.value(6).toInt();
    row["创建时间"] = query.value(7).toString();
    return row;
}

QJsonObject permissionRow(const QSqlQuery& query, bool checked)
{
    QJsonObject row;
    row["menuId"] = QString::number(query.value(0).toLongLong());
    row["permissionCode"] = query.value(1).toString();
    row["menuName"] = query.value(2).toString();
    row["moduleCode"] = query.value(3).toString();
    row["actionCode"] = query.value(4).toString();
    row["checked"] = checked;
    return row;
}

common::Response databaseUnavailable(DatabaseManager* database)
{
    if (!database->isEnabled()) {
        return {false, "当前为演示数据模式，权限配置需要连接 MySQL 后使用。", {}};
    }
    return {false, "MySQL 连接失败：" + database->lastError(), {}};
}

common::Response listUsers(DatabaseManager* database)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    QSqlQuery query(database->database());
    query.prepare(
        "SELECT u.id, u.username, u.real_name, u.phone, "
        "COALESCE(r.role_code, '') AS role_code, COALESCE(r.role_name, '') AS role_name, "
        "u.status, DATE_FORMAT(u.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
        "FROM sys_user u "
        "LEFT JOIN sys_user_role ur ON ur.user_id = u.id AND ur.is_primary = 1 "
        "LEFT JOIN sys_role r ON r.id = ur.role_id "
        "ORDER BY u.status DESC, u.id ASC");
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    QJsonArray rows;
    while (query.next()) {
        rows.append(userRow(query));
    }
    query.finish();

    return {true, "账号列表已加载。", QJsonObject{{"rows", rows}, {"count", rows.size()}}};
}

common::Response listRoles(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    QJsonArray roles;
    QSqlQuery roleQuery(database->database());
    roleQuery.prepare("SELECT id, role_code, role_name, data_scope, description, status "
                      "FROM sys_role ORDER BY id ASC");
    if (!roleQuery.exec()) {
        return {false, roleQuery.lastError().text(), {}};
    }
    while (roleQuery.next()) {
        QJsonObject role;
        role["roleId"] = QString::number(roleQuery.value(0).toLongLong());
        role["roleCode"] = roleQuery.value(1).toString();
        role["roleName"] = roleQuery.value(2).toString();
        role["dataScope"] = roleQuery.value(3).toString();
        role["description"] = roleQuery.value(4).toString();
        role["status"] = roleQuery.value(5).toInt();
        roles.append(role);
    }
    roleQuery.finish();

    QString roleId = payload.value("roleId").toVariant().toString().trimmed();
    if (roleId.isEmpty() && !roles.isEmpty()) {
        roleId = roles.first().toObject().value("roleId").toString();
    }

    QSet<QString> checkedPermissionCodes;
    if (!roleId.isEmpty()) {
        QSqlQuery checkedQuery(database->database());
        checkedQuery.prepare(
            "SELECT m.permission_code "
            "FROM sys_role_menu rm "
            "JOIN sys_menu m ON m.id = rm.menu_id "
            "WHERE rm.role_id = :role_id AND m.permission_code IS NOT NULL");
        checkedQuery.bindValue(":role_id", roleId.toLongLong());
        if (!checkedQuery.exec()) {
            return {false, checkedQuery.lastError().text(), {}};
        }
        while (checkedQuery.next()) {
            checkedPermissionCodes.insert(checkedQuery.value(0).toString());
        }
        checkedQuery.finish();
    }

    QJsonArray permissions;
    QSqlQuery permissionQuery(database->database());
    permissionQuery.prepare(
        "SELECT id, permission_code, menu_name, module_code, action_code "
        "FROM sys_menu "
        "WHERE status = 1 AND permission_code IS NOT NULL AND permission_code <> '' "
        "ORDER BY module_code, action_code, id");
    if (!permissionQuery.exec()) {
        return {false, permissionQuery.lastError().text(), {}};
    }
    while (permissionQuery.next()) {
        const QString code = permissionQuery.value(1).toString();
        permissions.append(permissionRow(permissionQuery, checkedPermissionCodes.contains(code)));
    }
    permissionQuery.finish();

    QJsonObject data;
    data["roles"] = roles;
    data["permissions"] = permissions;
    data["selectedRoleId"] = roleId;
    return {true, "角色权限已加载。", data};
}

qint64 roleIdByCode(DatabaseManager* database, const QString& roleCode, QString* roleName)
{
    QSqlQuery query(database->database());
    query.prepare("SELECT id, role_name FROM sys_role WHERE role_code = :role_code AND status = 1 LIMIT 1");
    query.bindValue(":role_code", roleCode.trimmed().toUpper());
    if (!query.exec() || !query.next()) {
        return 0;
    }
    const qint64 id = query.value(0).toLongLong();
    if (roleName) {
        *roleName = query.value(1).toString();
    }
    query.finish();
    return id;
}

QString usernameByUserId(DatabaseManager* database, qint64 userId)
{
    QSqlQuery query(database->database());
    query.prepare("SELECT username FROM sys_user WHERE id = :user_id LIMIT 1");
    query.bindValue(":user_id", userId);
    if (!query.exec() || !query.next()) {
        return {};
    }
    const QString username = query.value(0).toString();
    query.finish();
    return username;
}

bool upsertLegacyUser(QSqlDatabase& db,
                      const QString& username,
                      const QString& realName,
                      const QString& phone,
                      const QString& roleCode,
                      QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO users (username, password_hash, real_name, phone, role_id, status) "
        "SELECT :username, :password_hash, :real_name, :phone, r.id, 1 "
        "FROM roles r WHERE r.role_code = :role_code LIMIT 1 "
        "ON DUPLICATE KEY UPDATE real_name = VALUES(real_name), phone = VALUES(phone), "
        "role_id = VALUES(role_id), status = 1");
    query.bindValue(":username", username);
    query.bindValue(":password_hash", sha256("123456"));
    query.bindValue(":real_name", realName);
    query.bindValue(":phone", phone);
    query.bindValue(":role_code", roleCode);
    if (!query.exec()) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    query.finish();
    return true;
}

common::Response createUser(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    const QString username = payload.value("username").toString(payload.value("账号").toString()).trimmed();
    const QString realName = payload.value("realName").toString(payload.value("姓名").toString()).trimmed();
    const QString phone = payload.value("phone").toString(payload.value("电话").toString()).trimmed();
    const QString roleCode = payload.value("roleCode").toString(payload.value("角色编码").toString()).trimmed().toUpper();
    if (username.isEmpty() || realName.isEmpty() || roleCode.isEmpty()) {
        return {false, "账号、姓名和角色不能为空。", {}};
    }

    QString roleName;
    const qint64 roleId = roleIdByCode(database, roleCode, &roleName);
    if (roleId <= 0) {
        return {false, "未找到可用角色：" + roleCode, {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery upsert(db);
    upsert.prepare(
        "INSERT INTO sys_user (username, password_hash, real_name, phone, status) "
        "VALUES (:username, :password_hash, :real_name, :phone, 1) "
        "ON DUPLICATE KEY UPDATE real_name = VALUES(real_name), phone = VALUES(phone), status = 1");
    upsert.bindValue(":username", username);
    upsert.bindValue(":password_hash", sha256("123456"));
    upsert.bindValue(":real_name", realName);
    upsert.bindValue(":phone", phone);
    if (!upsert.exec()) {
        db.rollback();
        return {false, upsert.lastError().text(), {}};
    }
    upsert.finish();

    QSqlQuery userQuery(db);
    userQuery.prepare("SELECT id FROM sys_user WHERE username = :username LIMIT 1");
    userQuery.bindValue(":username", username);
    if (!userQuery.exec() || !userQuery.next()) {
        db.rollback();
        return {false, userQuery.lastError().text(), {}};
    }
    const qint64 userId = userQuery.value(0).toLongLong();
    userQuery.finish();

    QSqlQuery deleteRoles(db);
    deleteRoles.prepare("DELETE FROM sys_user_role WHERE user_id = :user_id");
    deleteRoles.bindValue(":user_id", userId);
    if (!deleteRoles.exec()) {
        db.rollback();
        return {false, deleteRoles.lastError().text(), {}};
    }
    deleteRoles.finish();

    QSqlQuery insertRole(db);
    insertRole.prepare("INSERT INTO sys_user_role (user_id, role_id, is_primary) VALUES (:user_id, :role_id, 1)");
    insertRole.bindValue(":user_id", userId);
    insertRole.bindValue(":role_id", roleId);
    if (!insertRole.exec()) {
        db.rollback();
        return {false, insertRole.lastError().text(), {}};
    }
    insertRole.finish();

    QString legacyError;
    if (!upsertLegacyUser(db, username, realName, phone, roleCode, &legacyError)) {
        db.rollback();
        return {false, legacyError, {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data;
    data["userId"] = QString::number(userId);
    data["roleName"] = roleName;
    return {true, "账号已保存，默认密码为 123456。", data};
}

common::Response resetPassword(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    const QString userId = payload.value("userId").toVariant().toString().trimmed();
    const QString username = payload.value("username").toString(payload.value("账号").toString()).trimmed();
    if (userId.isEmpty() && username.isEmpty()) {
        return {false, "请选择要重置密码的账号。", {}};
    }

    const QString resolvedUsername = username.isEmpty()
        ? usernameByUserId(database, userId.toLongLong())
        : username;

    QSqlQuery query(database->database());
    if (!userId.isEmpty()) {
        query.prepare("UPDATE sys_user SET password_hash = :password_hash WHERE id = :user_id");
        query.bindValue(":user_id", userId.toLongLong());
    } else {
        query.prepare("UPDATE sys_user SET password_hash = :password_hash WHERE username = :username");
        query.bindValue(":username", username);
    }
    query.bindValue(":password_hash", sha256("123456"));
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    query.finish();

    if (!resolvedUsername.isEmpty()) {
        QSqlQuery legacy(database->database());
        legacy.prepare("UPDATE users SET password_hash = :password_hash WHERE username = :username");
        legacy.bindValue(":password_hash", sha256("123456"));
        legacy.bindValue(":username", resolvedUsername);
        if (!legacy.exec()) {
            return {false, legacy.lastError().text(), {}};
        }
        legacy.finish();
    }

    return {true, "密码已重置为 123456。", {}};
}

common::Response toggleUser(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    const QString userId = payload.value("userId").toVariant().toString().trimmed();
    const int status = payload.value("status").toVariant().toInt();
    if (userId.isEmpty()) {
        return {false, "请选择要启用或停用的账号。", {}};
    }

    const QString username = usernameByUserId(database, userId.toLongLong());

    QSqlQuery query(database->database());
    query.prepare("UPDATE sys_user SET status = :status WHERE id = :user_id");
    query.bindValue(":status", status == 0 ? 0 : 1);
    query.bindValue(":user_id", userId.toLongLong());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    query.finish();

    if (!username.isEmpty()) {
        QSqlQuery legacy(database->database());
        legacy.prepare("UPDATE users SET status = :status WHERE username = :username");
        legacy.bindValue(":status", status == 0 ? 0 : 1);
        legacy.bindValue(":username", username);
        if (!legacy.exec()) {
            return {false, legacy.lastError().text(), {}};
        }
        legacy.finish();
    }

    return {true, status == 0 ? "账号已停用。" : "账号已启用。", {}};
}

common::Response saveRolePermissions(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return databaseUnavailable(database);
    }

    const QString roleId = payload.value("roleId").toVariant().toString().trimmed();
    if (roleId.isEmpty()) {
        return {false, "请选择要保存的角色。", {}};
    }

    QSet<QString> selectedCodes;
    const QJsonArray permissions = payload.value("permissions").toArray();
    for (const auto& value : permissions) {
        const QString code = value.toString().trimmed();
        if (!code.isEmpty()) {
            selectedCodes.insert(code);
        }
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM sys_role_menu WHERE role_id = :role_id");
    deleteQuery.bindValue(":role_id", roleId.toLongLong());
    if (!deleteQuery.exec()) {
        db.rollback();
        return {false, deleteQuery.lastError().text(), {}};
    }
    deleteQuery.finish();

    QSqlQuery menuQuery(db);
    menuQuery.prepare("SELECT id, permission_code FROM sys_menu "
                      "WHERE status = 1 AND permission_code IS NOT NULL AND permission_code <> ''");
    if (!menuQuery.exec()) {
        db.rollback();
        return {false, menuQuery.lastError().text(), {}};
    }

    while (menuQuery.next()) {
        const QString code = menuQuery.value(1).toString();
        if (!selectedCodes.contains(code)) {
            continue;
        }

        QSqlQuery insert(db);
        insert.prepare("INSERT INTO sys_role_menu (role_id, menu_id) VALUES (:role_id, :menu_id)");
        insert.bindValue(":role_id", roleId.toLongLong());
        insert.bindValue(":menu_id", menuQuery.value(0).toLongLong());
        if (!insert.exec()) {
            db.rollback();
            return {false, insert.lastError().text(), {}};
        }
        insert.finish();
    }
    menuQuery.finish();

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, "角色权限已保存，相关账号重新登录后生效。", {}};
}

} // namespace

PermissionAdminService::PermissionAdminService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response PermissionAdminService::handle(const common::Request& request)
{
    if (request.action == "users") {
        return listUsers(m_database);
    }
    if (request.action == "roles") {
        return listRoles(m_database, request.payload);
    }
    if (request.action == "createUser") {
        return createUser(m_database, request.payload);
    }
    if (request.action == "resetPassword") {
        return resetPassword(m_database, request.payload);
    }
    if (request.action == "toggleUser") {
        return toggleUser(m_database, request.payload);
    }
    if (request.action == "saveRolePermissions") {
        return saveRolePermissions(m_database, request.payload);
    }

    return {false, "Unsupported permissionAdmin action", {}};
}

} // namespace hospital::server
