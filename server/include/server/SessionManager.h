#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

namespace hospital { namespace server {

struct Session
{
    QString token;
    QString userId;
    QString username;
    QString realName;
    QString roleCode;
    QString roleName;
    QStringList permissions;
    QStringList menus;
    QString dataScope;
    QStringList departmentIds;
    QString primaryDeptId;
    QString doctorId;
    QString patientUserId;
    QString patientId;
    QString patientName;
    QString patientPhone;
    QString patientIdCard;
    QString userType;
};

class SessionManager
{
public:
    Session createSession(const QString& userId,
                          const QString& username,
                          const QString& realName,
                          const QString& roleCode,
                          const QString& roleName,
                          const QStringList& permissions = {},
                          const QStringList& menus = {},
                          const QString& dataScope = {},
                          const QStringList& departmentIds = {},
                          const QString& primaryDeptId = {},
                          const QString& doctorId = {});
    Session createPatientSession(const QString& patientId,
                                 const QString& username,
                                 const QString& patientName,
                                 const QString& phone = {},
                                 const QString& idCard = {},
                                 const QString& selectedPatientId = {});
    bool findSession(const QString& token, Session* session) const;
    bool isLoggedIn(const QString& token) const;
    void invalidateSession(const QString& token);

private:
    mutable QMutex m_mutex;
    QHash<QString, Session> m_sessions;
};

}} // namespace hospital::server
