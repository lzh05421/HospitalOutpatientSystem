#include "server/SessionManager.h"

#include <QMutexLocker>
#include <QUuid>

namespace hospital::server {

Session SessionManager::createSession(const QString& userId,
                                      const QString& username,
                                      const QString& realName,
                                      const QString& roleCode,
                                      const QString& roleName,
                                      const QStringList& permissions,
                                      const QStringList& menus,
                                      const QString& dataScope,
                                      const QStringList& departmentIds,
                                      const QString& primaryDeptId,
                                      const QString& doctorId)
{
    Session session;
    session.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.userId = userId;
    session.username = username;
    session.realName = realName;
    session.roleCode = roleCode;
    session.roleName = roleName;
    session.permissions = permissions;
    session.menus = menus;
    session.dataScope = dataScope;
    session.departmentIds = departmentIds;
    session.primaryDeptId = primaryDeptId;
    session.doctorId = doctorId;
    session.userType = "STAFF";

    QMutexLocker locker(&m_mutex);
    m_sessions.insert(session.token, session);
    return session;
}

Session SessionManager::createPatientSession(const QString& patientId,
                                             const QString& username,
                                             const QString& patientName,
                                             const QString& phone,
                                             const QString& idCard,
                                             const QString& selectedPatientId)
{
    Session session;
    session.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.userId = patientId;
    session.username = username;
    session.realName = patientName;
    session.roleCode = "PATIENT";
    session.roleName = "患者";
    session.permissions = {"registration:create", "registration:insurancePrecheck",
                           "registration:insuranceProfile", "registration:saveInsuranceProfile", "registration:history",
                           "auth:patientListMembers", "auth:patientAddMember",
                           "billing:createPaymentQr", "billing:checkPayStatus",
                           "billing:medicalInsurancePay", "billing:pay"};
    session.menus = {"registration", "billing"};
    session.dataScope = "SELF";
    session.patientUserId = patientId;
    session.patientId = selectedPatientId.isEmpty() ? patientId : selectedPatientId;
    session.patientName = patientName;
    session.patientPhone = phone;
    session.patientIdCard = idCard;
    session.userType = "PATIENT";

    QMutexLocker locker(&m_mutex);
    m_sessions.insert(session.token, session);
    return session;
}

bool SessionManager::findSession(const QString& token, Session* session) const
{
    if (token.trimmed().isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const auto it = m_sessions.constFind(token);
    if (it == m_sessions.constEnd()) {
        return false;
    }
    if (session) {
        *session = it.value();
    }
    return true;
}

bool SessionManager::isLoggedIn(const QString& token) const
{
    return findSession(token, nullptr);
}

void SessionManager::invalidateSession(const QString& token)
{
    QMutexLocker locker(&m_mutex);
    m_sessions.remove(token);
}

} // namespace hospital::server
