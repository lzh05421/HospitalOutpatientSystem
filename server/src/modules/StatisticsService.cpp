#include "server/modules/ModuleServices.h"

#include "server/SqlJson.h"

namespace hospital::server {

StatisticsService::StatisticsService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response StatisticsService::handle(const common::Request& request)
{
    if (request.action != "daily") {
        return {false, "Unsupported statistics action", {}};
    }

    return SqlJson::selectRows(m_database,
        "SELECT s.stat_date AS '统计日期', COALESCE(d.dept_name, '全院') AS '科室', "
        "s.registration_income AS '挂号收入', s.drug_income AS '药品收入', "
        "s.total_income AS '总收入', "
        "CASE WHEN s.total_income > 0 THEN CONCAT(ROUND(s.drug_income / s.total_income * 100, 1), '%') ELSE '0%' END AS '药品占比' "
        "FROM fee_statistics_daily s "
        "LEFT JOIN departments d ON d.id = s.department_id "
        "ORDER BY s.stat_date DESC LIMIT 100",
        {}, "statistics");
}

} // namespace hospital::server
