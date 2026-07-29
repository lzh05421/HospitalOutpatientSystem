#pragma once

#include "server/RequestRouter.h"

namespace hospital { namespace server {

class DatabaseManager;
class SessionManager;

class AuthService : public ModuleService
{
public:
    AuthService(DatabaseManager* database, SessionManager* sessions);
    common::Response handle(const common::Request& request) override;

private:
    common::Response login(const common::Request& request);
    common::Response patientLogin(const common::Request& request);
    common::Response patientRegister(const common::Request& request);
    common::Response patientListMembers(const common::Request& request);
    common::Response patientAddMember(const common::Request& request);
    common::Response responseForSession(const QString& username,
                                        const QString& userId,
                                        const QString& realName,
                                        const QString& roleCode,
                                        const QString& roleName) const;
    common::Response patientResponseForSession(const QString& patientUserId,
                                               const QString& username,
                                               const QString& name,
                                               const QString& phone,
                                               const QString& idCard,
                                               const QString& selectedPatientId = QString()) const;

    DatabaseManager* m_database = nullptr;
    SessionManager* m_sessions = nullptr;
};

}} // namespace hospital::server
