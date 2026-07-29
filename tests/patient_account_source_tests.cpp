#include <fstream>
#include <iterator>
#include <string>
#include <vector>

std::string readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

bool containsAll(const std::string& source, const std::vector<std::string>& fragments)
{
    for (const auto& fragment : fragments) {
        if (source.find(fragment) == std::string::npos) {
            return false;
        }
    }
    return true;
}

int main()
{
    const std::string schema = readFile("database/schema.sql");
    const std::string migration = readFile("database/migrations/migration_patient_accounts.sql");
    const std::string databaseManager = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(schema + migration + databaseManager, {
            "CREATE TABLE patient_users",
            "username VARCHAR(64)",
            "password_hash VARCHAR(255)",
            "password_salt VARCHAR(64)",
            "user_id BIGINT",
            "relationship VARCHAR(32)",
            "fk_patients_user",
            "ALTER TABLE registrations ADD COLUMN user_id BIGINT",
            "ALTER TABLE bills ADD COLUMN user_id BIGINT",
            "uk_patients_username",
            "SHA2(CONCAT(password_salt, ':123456'), 256)"
        })) {
        return 1;
    }

    const std::string authHeader = readFile("server/include/server/modules/AuthService.h");
    const std::string authSource = readFile("server/src/modules/AuthService.cpp");
    if (!containsAll(authHeader + authSource, {
            "patientLogin",
            "patientRegister",
            "patientResponseForSession",
            "patientListMembers",
            "patientAddMember",
            "password_salt",
            "QRegularExpression",
            "^\\\\d{17}[0-9Xx]$",
            "手机号已注册",
            "createPatientSession"
        })) {
        return 2;
    }

    const std::string apiHeader = readFile("client/include/client/ApiClient.h");
    const std::string apiSource = readFile("client/src/ApiClient.cpp");
    const std::string patientManagerHeader = readFile("client/include/client/PatientManager.h");
    const std::string patientManagerSource = readFile("client/src/PatientManager.cpp");
    if (!containsAll(apiHeader + apiSource + patientManagerHeader + patientManagerSource, {
            "setPatientSession",
            "isPatientLoggedIn",
            "patientUserId",
            "patientId",
            "patientName",
            "patientIdCard",
            "patientPhone",
            "PatientManager",
            "loadPatients",
            "addPatient",
            "selectPatient",
            "currentPatientChanged",
            "loadMyHistory"
        })) {
        return 3;
    }

    const std::string entryHeader = readFile("client/include/client/EntryDialog.h");
    const std::string entrySource = readFile("client/src/EntryDialog.cpp");
    const std::string loginHeader = readFile("client/include/client/PatientLoginDialog.h");
    const std::string loginSource = readFile("client/src/PatientLoginDialog.cpp");
    const std::string userCenterHeader = readFile("client/include/client/UserCenterWindow.h");
    const std::string userCenterSource = readFile("client/src/UserCenterWindow.cpp");
    if (!containsAll(entryHeader + entrySource + loginHeader + loginSource + userCenterHeader + userCenterSource, {
            "PatientLogin",
            "患者登录/注册",
            "PatientLoginDialog",
            "账号密码登录",
            "新用户注册",
            "patientLogin",
            "patientRegister",
            "UserCenterWindow",
            "QTabWidget",
            "挂号记录",
            "处方单",
            "缴费记录",
            "切换就诊人"
        })) {
        return 4;
    }

    const std::string registration = readFile("server/src/modules/RegistrationService.cpp");
    if (!containsAll(registration, {
            "__patientId",
            "__patientUserId",
            "user_id = :user_id",
            "patient_id = :patient_id",
            "r.patient_id = :patient_id",
            "r.user_id = :user_id",
            "my-history",
            "待支付",
            "已完成",
            "已取消"
        })) {
        return 5;
    }

    const std::string billing = readFile("server/src/modules/BillingService.cpp");
    if (!containsAll(billing, {
            "__patientId",
            "__patientUserId",
            "b.user_id = :user_id",
            "b.patient_id = :patient_id",
            "禁止越权"
        })) {
        return 6;
    }

    const std::string window = readFile("client/src/PatientAppointmentWindow.cpp");
    if (!containsAll(window, {
            "ensurePatientLoggedIn",
            "currentPatientCard",
            "showPatientSwitcher",
            "添加新就诊人",
            "setReadOnly(true)",
            "patientName",
            "patientPhone",
            "patientIdCard",
            "请先登录",
            "查看历史订单",
            "openUserCenter",
            "connect(userCenterButton, &QPushButton::clicked, this, &PatientAppointmentWindow::openUserCenter)",
            "connect(historyButton, &QPushButton::clicked, this, &PatientAppointmentWindow::requestOrderHistory)"
        })) {
        return 7;
    }

    return 0;
}
