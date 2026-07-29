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
    const std::string header = readFile("client/include/client/pages/Pages.h");
    const std::string schedulePage = readFile("client/src/pages/SchedulePage.cpp");
    const std::string scheduleService = readFile("server/src/modules/ScheduleService.cpp");

    if (!containsAll(header + schedulePage, {
            "shouldDoctorWorkOnRotation",
            "hasActiveClinicCoverage",
            "doctorsForClinic",
            "generatedCoverageKeys",
            "hasRotationCoverage",
            "上三休一",
            "周六",
            "周日",
            "dayOffset % 4",
            "startDate.addDays(dayOffset)"
        })) {
        return 1;
    }

    if (!containsAll(schedulePage, {
            "const QDate date = startDate.addDays(dayOffset)",
            "bool hasRotationCoverage = false",
            "&& !hasRotationCoverage",
            "if (!shouldDoctorWorkOnRotation(name, dayOffset, clinicDoctors))",
            "generatedCoverageKeys.insert(coverageKey)",
            "if (!hasActiveClinicCoverage(existingScheduleRows, clinic, dateText)",
            "医生上三天休息一天，休息错开；周六、周日也参与排班；每天每个门诊至少保留一名医生。"
        })) {
        return 2;
    }

    if (schedulePage.find("int remaining = qMax(0, 5 - scheduledHalfDaysInRows") != std::string::npos) {
        return 3;
    }
    if (schedulePage.find("将从开始日期起自动补齐 7 天排班：每位医生最多 5 天") != std::string::npos) {
        return 4;
    }

    if (!containsAll(scheduleService, {
            "ORDER BY s.work_date ASC",
            "智能排班从用户选择的开始日期优先展示"
        })) {
        return 5;
    }

    return 0;
}
