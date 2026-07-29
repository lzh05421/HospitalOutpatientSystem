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
    const std::string database = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(schema + database, {
            "is_emergency",
            "emergency_reason",
            "ALTER TABLE registrations ADD COLUMN is_emergency",
            "ALTER TABLE registrations ADD COLUMN emergency_reason",
            "registration_mark_emergency",
            "registration:markEmergency"
        })) {
        return 1;
    }

    const std::string service = readFile("server/src/modules/RegistrationService.cpp");
    const std::string consultation = readFile("server/src/modules/ConsultationService.cpp");
    if (!containsAll(service + consultation, {
            "markEmergencyRegistrationInDatabase",
            "request.action == \"markEmergency\"",
            "SET is_emergency = 1, emergency_reason = :emergency_reason",
            "payload.value(\"isEmergency\")",
            "is_emergency, emergency_reason",
            ":is_emergency, :emergency_reason",
            "急诊标识",
            "急诊原因",
            "CASE WHEN r.is_emergency = 1 THEN 0 ELSE 1 END",
            "急诊优先"
        })) {
        return 2;
    }

    const std::string waitingPage = readFile("client/src/pages/WaitingQueuePage.cpp");
    const std::string appointmentPage = readFile("client/src/PatientAppointmentWindow.cpp");
    const std::string appointmentHeader = readFile("client/include/client/PatientAppointmentWindow.h");
    const std::string pagesHeader = readFile("client/include/client/pages/Pages.h");
    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(waitingPage + appointmentPage + appointmentHeader + pagesHeader + modulePage, {
            "markSelectedEmergency",
            "设为急诊优先",
            "m_emergencyCheckBox",
            "m_emergencyReasonEdit",
            "急诊挂号",
            "急诊原因",
            "request.payload[\"isEmergency\"]",
            "request.payload[\"emergencyReason\"]",
            "request.action = \"markEmergency\"",
            "\"急诊标识\"",
            "\"急诊原因\""
        })) {
        return 3;
    }

    return 0;
}
