#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QStringList>
#include <QVariantMap>

namespace hospital::server {

OperationLogService::OperationLogService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response OperationLogService::handle(const common::Request& request)
{
    if (request.action != "list") {
        return {false, "Unsupported operation log action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    if (!m_database->isEnabled()) {
        const auto rows = DemoRepository::instance().rows("operationLogs", keyword);
        QJsonObject data;
        data["rows"] = rows;
        data["count"] = rows.size();
        return {true, "OK", data};
    }

    QStringList filters;
    QVariantMap params;
    if (!keyword.isEmpty()) {
        filters.append("(COALESCE(u.real_name, '') LIKE CONCAT('%', :keyword_operator, '%') "
                       "OR l.module LIKE CONCAT('%', :keyword_module, '%') "
                       "OR l.action LIKE CONCAT('%', :keyword_action, '%') "
                       "OR COALESCE(l.content, '') LIKE CONCAT('%', :keyword_content, '%'))");
        params.insert("keyword_operator", keyword);
        params.insert("keyword_module", keyword);
        params.insert("keyword_action", keyword);
        params.insert("keyword_content", keyword);
    }
    const QString whereSql = filters.isEmpty() ? QString() : "WHERE " + filters.join(" AND ") + " ";

    return SqlJson::selectRows(m_database,
        "SELECT COALESCE(u.real_name, '系统') AS '操作人', l.module AS '模块', "
        "l.action AS '动作', COALESCE(l.content, '') AS '内容', l.created_at AS '操作时间' "
        "FROM operation_logs l "
        "LEFT JOIN users u ON u.id = l.user_id "
        + whereSql +
        "ORDER BY l.created_at DESC LIMIT 200",
        params, "operationLogs");
}

} // namespace hospital::server
