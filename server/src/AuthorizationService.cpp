#include "server/AuthorizationService.h"

#include "server/DatabaseManager.h"
#include "server/SessionManager.h"

#include <QHash>
#include <QSqlQuery>
#include <QVariant>

namespace hospital::server {
namespace {

QStringList normalized(QStringList values)
{
    values.removeAll({});
    values.removeDuplicates();
    values.sort();
    return values;
}

QStringList rolePreset(const QString& roleCode)
{
    static const QHash<QString, QStringList> presets = {
        {"REGISTRAR", {
            "patient:list", "patient:update", "patient:delete",
            "registration:list", "registration:create", "registration:insurancePrecheck",
            "registration:insuranceProfile", "registration:saveInsuranceProfile", "registration:update", "registration:delete",
            "registration:waiting", "registration:call", "registration:markEmergency", "registration:history",
            "billing:pay", "billing:medicalInsurancePay", "billing:createPaymentQr", "billing:checkPayStatus",
            "schedule:list", "schedule:rangeList", "schedule:save", "schedule:update", "schedule:delete",
            "schedule:reset", "schedule:batchSave", "schedule:rulesList", "schedule:rulesSaveAll",
            "department:list", "doctor:list",
            "patientRecord:list"
        }},
        {"DOCTOR", {
            "patient:list",
            "registration:waiting", "registration:call", "registration:markEmergency",
            "consultation:list", "consultation:start", "consultation:save", "consultation:saveWaiting",
            "examination:list", "examination:items", "examination:create", "examination:complete",
            "prescription:list", "prescription:create",
            "patientRecord:list", "patientRecord:update",
            "doctor:list"
        }},
        {"DIRECTOR", {
            "dashboard:summary", "dashboard:stats", "dashboard:topDoctors", "statistics:daily",
            "patient:list", "patient:update", "patient:delete",
            "registration:list", "registration:waiting", "registration:call", "registration:markEmergency",
            "consultation:list", "consultation:start", "consultation:save", "consultation:saveWaiting",
            "examination:list", "examination:items", "examination:saveItem", "examination:deleteItem",
            "examination:create", "examination:complete",
            "prescription:list", "prescription:create", "prescription:review", "prescription:reject", "prescription:dispense", "prescription:return",
            "patientRecord:list", "patientRecord:update", "patientRecord:delete",
            "doctor:list", "doctor:create", "doctor:update", "doctor:delete",
            "schedule:list", "schedule:rangeList", "schedule:save", "schedule:update", "schedule:delete",
            "schedule:reset", "schedule:batchSave", "schedule:rulesList", "schedule:rulesSaveAll"
        }},
        {"PHARMACIST", {
            "prescription:list", "prescription:review", "prescription:reject", "prescription:dispense", "prescription:return",
            "inventory:list", "inventory:inbound", "inventory:update", "inventory:delete"
        }},
        {"CASHIER", {
            "billing:list", "billing:pay", "billing:medicalInsurancePay", "billing:createPaymentQr", "billing:checkPayStatus",
            "billing:refund", "billing:requestRefund", "billing:update",
            "statistics:daily", "dashboard:summary", "dashboard:stats", "dashboard:topDoctors"
        }},
        {"ADMIN", {
            "operationLog:list",
            "examination:items", "examination:saveItem", "examination:deleteItem",
            "billing:list", "billing:requestRefund", "billing:reviewRefund",
            "permissionAdmin:users", "permissionAdmin:roles", "permissionAdmin:createUser",
            "permissionAdmin:resetPassword", "permissionAdmin:toggleUser", "permissionAdmin:saveRolePermissions"
        }}
    };

    return normalized(presets.value(roleCode.toUpper()));
}

QStringList allPresetPermissions()
{
    QStringList values;
    for (const auto& role : {"REGISTRAR", "DOCTOR", "DIRECTOR", "PHARMACIST", "CASHIER", "ADMIN"}) {
        values.append(rolePreset(QString::fromLatin1(role)));
    }
    return normalized(values);
}

QStringList menuCodesForPermissions(const QStringList& permissions)
{
    QStringList menus;
    for (const QString& permission : permissions) {
        const int separator = permission.indexOf(':');
        if (separator > 0) {
            menus.append(permission.left(separator));
        }
    }
    return normalized(menus);
}

QString defaultDataScopeForRole(const QString& roleCode)
{
    const QString upper = roleCode.toUpper();
    if (upper == "ADMIN" || upper == "REGISTRAR" || upper == "CASHIER") {
        return "ALL";
    }
    if (upper == "DIRECTOR") {
        return "DEPT";
    }
    if (upper == "DOCTOR") {
        return "SELF";
    }
    if (upper == "PHARMACIST") {
        return "PHARMACY_PENDING";
    }
    return "SELF";
}

QString scopeWithHigherPriority(const QString& current, const QString& candidate)
{
    static const QHash<QString, int> priority = {
        {"ALL", 4},
        {"CUSTOM", 3},
        {"DEPT", 2},
        {"DEPARTMENT", 2},
        {"PHARMACY_PENDING", 2},
        {"SELF", 1}
    };

    const QString normalizedCurrent = current.toUpper();
    const QString normalizedCandidate = candidate.toUpper();
    return priority.value(normalizedCandidate, 0) > priority.value(normalizedCurrent, 0)
        ? normalizedCandidate
        : normalizedCurrent;
}

void applyRbacTables(DatabaseManager* database,
                     const QString& userId,
                     const QString& username,
                     AuthorizationProfile* profile)
{
    if (!database || !database->isEnabled() || !database->ensureOpen() || !profile) {
        return;
    }

    const qint64 numericUserId = userId.toLongLong();
    QSqlQuery roleBindingQuery(database->database());
    roleBindingQuery.prepare(
        "SELECT COUNT(*) "
        "FROM sys_user u "
        "JOIN sys_user_role ur ON ur.user_id = u.id "
        "JOIN sys_role r ON r.id = ur.role_id "
        "WHERE (u.id = :user_id OR u.username = :username) "
        "AND u.status = 1 AND r.status = 1");
    roleBindingQuery.bindValue(":user_id", numericUserId);
    roleBindingQuery.bindValue(":username", username);
    const bool hasActiveRbacRole = roleBindingQuery.exec()
        && roleBindingQuery.next()
        && roleBindingQuery.value(0).toInt() > 0;
    roleBindingQuery.finish();
    if (!hasActiveRbacRole) {
        return;
    }

    QSqlQuery permissionQuery(database->database());
    permissionQuery.prepare(
        "SELECT DISTINCT m.permission_code "
        "FROM sys_user u "
        "JOIN sys_user_role ur ON ur.user_id = u.id "
        "JOIN sys_role r ON r.id = ur.role_id "
        "JOIN sys_role_menu rm ON rm.role_id = r.id "
        "JOIN sys_menu m ON m.id = rm.menu_id "
        "WHERE (u.id = :user_id OR u.username = :username) "
        "AND u.status = 1 AND r.status = 1 AND m.status = 1 "
        "AND m.permission_code IS NOT NULL AND m.permission_code <> ''");
    permissionQuery.bindValue(":user_id", numericUserId);
    permissionQuery.bindValue(":username", username);
    QStringList tablePermissions;
    if (!permissionQuery.exec()) {
        permissionQuery.finish();
        return;
    }
    while (permissionQuery.next()) {
        tablePermissions.append(permissionQuery.value(0).toString());
    }
    permissionQuery.finish();
    tablePermissions = normalized(tablePermissions);
    profile->permissionsLoadedFromRbac = true;
    profile->permissions = tablePermissions;
    profile->menus = menuCodesForPermissions(tablePermissions);

    QSqlQuery scopeQuery(database->database());
    scopeQuery.prepare(
        "SELECT DISTINCT r.data_scope "
        "FROM sys_user u "
        "JOIN sys_user_role ur ON ur.user_id = u.id "
        "JOIN sys_role r ON r.id = ur.role_id "
        "WHERE (u.id = :user_id OR u.username = :username) "
        "AND u.status = 1 AND r.status = 1 "
        "AND r.data_scope IS NOT NULL AND r.data_scope <> ''");
    scopeQuery.bindValue(":user_id", numericUserId);
    scopeQuery.bindValue(":username", username);
    QString resolvedScope = profile->dataScope;
    if (scopeQuery.exec()) {
        while (scopeQuery.next()) {
            resolvedScope = scopeWithHigherPriority(resolvedScope, scopeQuery.value(0).toString());
        }
    }
    scopeQuery.finish();
    if (!resolvedScope.isEmpty()) {
        profile->dataScope = resolvedScope == "DEPARTMENT" ? "DEPT" : resolvedScope;
    }

    QSqlQuery deptQuery(database->database());
    deptQuery.prepare(
        "SELECT DISTINCT sud.dept_id, sud.is_primary "
        "FROM sys_user u "
        "JOIN sys_user_dept sud ON sud.user_id = u.id "
        "WHERE (u.id = :user_id OR u.username = :username)");
    deptQuery.bindValue(":user_id", numericUserId);
    deptQuery.bindValue(":username", username);
    QStringList departmentIds;
    QString primaryDeptId = profile->primaryDeptId;
    if (deptQuery.exec()) {
        while (deptQuery.next()) {
            const QString deptId = deptQuery.value(0).toString();
            departmentIds.append(deptId);
            if (deptQuery.value(1).toInt() == 1) {
                primaryDeptId = deptId;
            }
        }
    }
    deptQuery.finish();
    departmentIds = normalized(departmentIds);
    if (!departmentIds.isEmpty()) {
        profile->departmentIds = departmentIds;
        profile->primaryDeptId = primaryDeptId.isEmpty() ? departmentIds.first() : primaryDeptId;
    }
}

void applyLegacyDoctorScope(DatabaseManager* database,
                            const QString& userId,
                            AuthorizationProfile* profile)
{
    if (!database || !database->isEnabled() || !database->ensureOpen() || !profile) {
        return;
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT id, department_id FROM doctors WHERE user_id = :user_id AND status = 1 LIMIT 1");
    query.bindValue(":user_id", userId.toLongLong());
    if (query.exec() && query.next()) {
        if (profile->doctorId.isEmpty()) {
            profile->doctorId = query.value(0).toString();
        }
        if (profile->departmentIds.isEmpty()) {
            profile->departmentIds = {query.value(1).toString()};
        }
        if (profile->primaryDeptId.isEmpty()) {
            profile->primaryDeptId = query.value(1).toString();
        }
    }
    query.finish();
}

} // namespace

QString AuthorizationService::permissionCode(const QString& module, const QString& action)
{
    return module + ":" + action;
}

QStringList AuthorizationService::defaultPermissionsForRole(const QString& roleCode)
{
    const QString upper = roleCode.toUpper();
    if (upper == "ADMIN") {
        return allPresetPermissions();
    }
    return rolePreset(upper);
}

AuthorizationProfile AuthorizationService::defaultProfileForRole(const QString& roleCode)
{
    AuthorizationProfile profile;
    profile.permissions = defaultPermissionsForRole(roleCode);
    profile.menus = menuCodesForPermissions(profile.permissions);
    profile.dataScope = defaultDataScopeForRole(roleCode);
    return profile;
}

AuthorizationProfile AuthorizationService::profileForUser(DatabaseManager* database,
                                                          const QString& userId,
                                                          const QString& username,
                                                          const QString& roleCode)
{
    AuthorizationProfile profile = defaultProfileForRole(roleCode);
    applyRbacTables(database, userId, username, &profile);
    applyLegacyDoctorScope(database, userId, &profile);
    if (profile.permissionsLoadedFromRbac && roleCode.toUpper() == "ADMIN") {
        profile.permissions.append({
            "operationLog:list",
            "permissionAdmin:users",
            "permissionAdmin:roles",
            "permissionAdmin:createUser",
            "permissionAdmin:resetPassword",
            "permissionAdmin:toggleUser",
            "permissionAdmin:saveRolePermissions"
        });
        profile.menus = menuCodesForPermissions(profile.permissions);
    }
    profile.permissions = normalized(profile.permissions);
    profile.menus = normalized(profile.menus);
    profile.departmentIds = normalized(profile.departmentIds);
    return profile;
}

bool AuthorizationService::canAccess(const Session& session, const QString& module, const QString& action)
{
    if (session.userType == "PATIENT" || session.roleCode.toUpper() == "PATIENT") {
        const QString permission = permissionCode(module, action);
        return session.permissions.contains(permission);
    }

    const QString permission = permissionCode(module, action);
    QStringList permissions = session.permissions;
    if (session.roleCode.toUpper() == "ADMIN") {
        permissions.append({
            "operationLog:list",
            "permissionAdmin:users",
            "permissionAdmin:roles",
            "permissionAdmin:createUser",
            "permissionAdmin:resetPassword",
            "permissionAdmin:toggleUser",
            "permissionAdmin:saveRolePermissions"
        });
        permissions.removeDuplicates();
    }
    return permissions.contains(permission)
        || permissions.contains(module + ":*")
        || permissions.contains("*:*");
}

} // namespace hospital::server
