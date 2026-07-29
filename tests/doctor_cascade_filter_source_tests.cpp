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

std::string functionBody(const std::string& source, const std::string& signature)
{
    const auto start = source.find(signature);
    if (start == std::string::npos) {
        return {};
    }

    const auto brace = source.find('{', start);
    if (brace == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (auto index = brace; index < source.size(); ++index) {
        if (source[index] == '{') {
            ++depth;
        } else if (source[index] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(brace, index - brace + 1);
            }
        }
    }
    return {};
}

int main()
{
    const std::string apiClientHeader = readFile("client/include/client/ApiClient.h");
    const std::string apiClientSource = readFile("client/src/ApiClient.cpp");
    if (!containsAll(apiClientHeader, {
            "QString doctorId() const",
            "QString m_doctorId"
        })
        || !containsAll(apiClientSource, {
            "QString ApiClient::doctorId() const",
            "m_doctorId = data.value(\"doctorId\").toVariant().toString()",
            "m_doctorId.clear()"
        })) {
        return 8;
    }

    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(modulePage, {
            "request.payload[\"departmentFilter\"]",
            "request.payload[\"doctorFilter\"]",
            "request.payload[\"clinicTypeFilter\"]",
            "m_cascadeFilterDoctors",
            "refresh();",
            "usesDoctorSelfScope()",
            "loggedInDoctorFilterName()",
            "m_doctorFilterBox->setEnabled(false)"
        })) {
        return 1;
    }

    const std::string cascadeFilterRows = functionBody(modulePage, "QJsonArray ModulePage::cascadeFilterRows() const");
    if (cascadeFilterRows.empty()
        || cascadeFilterRows.find("searchedRows()") != std::string::npos
        || cascadeFilterRows.find("m_cascadeFilterDoctors") == std::string::npos) {
        return 5;
    }

    const std::string changeDoctorFilter = functionBody(modulePage, "void ModulePage::changeDoctorFilter(int)");
    if (changeDoctorFilter.empty()
        || changeDoctorFilter.find("m_selectedDoctorFilter =") == std::string::npos
        || changeDoctorFilter.find("refresh();") == std::string::npos
        || changeDoctorFilter.find("rebuildGroupFilter();") != std::string::npos) {
        return 4;
    }

    const std::string consultationService = readFile("server/src/modules/ConsultationService.cpp");
    if (!containsAll(consultationService, {
            "departmentFilter",
            "doctorFilter",
            "clinicTypeFilter",
            "activeConsultations(keyword, departmentFilter, doctorFilter, clinicTypeFilter)",
            "__doctorId",
            "r.doctor_id = :operator_doctor_id",
            "当前医生账号未绑定医生档案",
            "当前医生账号不能操作其他医生的接诊记录",
            "d.dept_name = :department_filter",
            "u.real_name = :doctor_filter",
            "clinic_type_filter"
        })) {
        return 2;
    }
    if (consultationService.find("u.real_name = :operator_name") != std::string::npos) {
        return 6;
    }

    const std::string registrationService = readFile("server/src/modules/RegistrationService.cpp");
    if (!containsAll(registrationService, {
            "departmentFilter",
            "doctorFilter",
            "clinicTypeFilter",
            "waitingQueue(keyword, departmentFilter, doctorFilter, clinicTypeFilter)",
            "__doctorId",
            "r.doctor_id = :operator_doctor_id",
            "当前医生账号未绑定医生档案",
            "d.dept_name = :department_filter",
            "u.real_name = :doctor_filter",
            "clinic_type_filter"
        })) {
        return 3;
    }
    if (registrationService.find("u.real_name = :operator_name") != std::string::npos) {
        return 7;
    }

    return 0;
}
