#pragma once

#include <QString>
#include <QStringList>

namespace hospital { namespace server {

class DatabaseManager;
struct Session;

struct AuthorizationProfile
{
    QStringList permissions;
    QStringList menus;
    QString dataScope;
    QStringList departmentIds;
    QString primaryDeptId;
    QString doctorId;
    bool permissionsLoadedFromRbac = false;
};

class AuthorizationService
{
public:
    static QString permissionCode(const QString& module, const QString& action);
    static QStringList defaultPermissionsForRole(const QString& roleCode);
    static AuthorizationProfile defaultProfileForRole(const QString& roleCode);
    static AuthorizationProfile profileForUser(DatabaseManager* database,
                                               const QString& userId,
                                               const QString& username,
                                               const QString& roleCode);
    static bool canAccess(const Session& session, const QString& module, const QString& action);
};

}} // namespace hospital::server
