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
    const std::string service = readFile("server/src/modules/DashboardService.cpp");
    if (!containsAll(service, {
            "startDate",
            "endDate",
            "dailyVisits",
            "departmentVisits",
            "doctorRanking",
            "DATE(r.register_time) BETWEEN :start_date AND :end_date",
            "COUNT(*) AS '门诊量'",
            "COUNT(*) AS '接诊量'",
            "d.dept_name AS '科室'",
            "u.real_name AS '医生'"
        })) {
        return 1;
    }

    const std::string page = readFile("client/src/pages/DashboardPage.cpp");
    const std::string header = readFile("client/include/client/pages/Pages.h");
    if (!containsAll(page + header, {
            "DashboardChartWidget",
            "QDateEdit",
            "m_startDateEdit",
            "m_endDateEdit",
            "exportDashboardCsv",
            "日门诊量趋势",
            "各科室接诊量",
            "医生接诊排行",
            "dailyVisits",
            "departmentVisits",
            "doctorRanking",
            "startDate",
            "endDate",
            "QPainter",
            "drawPolyline",
            "drawRect",
            "getSaveFileName",
            "驾驶舱报表",
            "out << QChar(0xFEFF)",
            "csvEscape"
        })) {
        return 2;
    }

    return 0;
}
