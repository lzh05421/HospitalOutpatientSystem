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
    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(modulePage, {
            "m_dateFilterBox->setCurrentText(\"今天\")",
            "request.payload[\"dateFilter\"]",
            "request.payload[\"dateValue\"]",
            "matchesDateFilter(row)",
            "refresh();"
        })) {
        return 1;
    }

    const std::string registrationService = readFile("server/src/modules/RegistrationService.cpp");
    if (!containsAll(registrationService, {
            "applyVisitDateFilter(filters, params, request.payload)",
            "s.work_date = :visit_date",
            "s.work_date BETWEEN :visit_start_date AND :visit_end_date"
        })) {
        return 2;
    }

    const std::string consultationService = readFile("server/src/modules/ConsultationService.cpp");
    if (!containsAll(consultationService, {
            "applyVisitDateFilter(filters, params, request.payload)",
            "s.work_date = :visit_date",
            "s.work_date BETWEEN :visit_start_date AND :visit_end_date"
        })) {
        return 3;
    }

    return 0;
}
