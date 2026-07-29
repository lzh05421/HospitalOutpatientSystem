#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace hospital::server {
namespace {

bool scalar(DatabaseManager* database, const QString& sql, QJsonValue* value)
{
    QSqlQuery query(database->database());
    query.prepare(sql);
    if (!query.exec() || !query.next()) {
        return false;
    }
    *value = QJsonValue::fromVariant(query.value(0));
    query.finish();
    return true;
}

bool scalarBound(DatabaseManager* database, const QString& sql, const QDate& startDate, const QDate& endDate, QJsonValue* value)
{
    QSqlQuery query(database->database());
    query.prepare(sql);
    query.bindValue(":start_date", startDate);
    query.bindValue(":end_date", endDate);
    if (!query.exec() || !query.next()) {
        return false;
    }
    *value = QJsonValue::fromVariant(query.value(0));
    query.finish();
    return true;
}

QJsonValue scalarOrDefault(DatabaseManager* database, const QString& sql, const QJsonValue& fallback)
{
    QJsonValue value;
    return scalar(database, sql, &value) ? value : fallback;
}

bool tableExists(DatabaseManager* database, const QString& tableName)
{
    QSqlQuery query(database->database());
    query.prepare("SELECT COUNT(*) FROM information_schema.TABLES "
                  "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = :table_name");
    query.bindValue(":table_name", tableName);
    const bool exists = query.exec() && query.next() && query.value(0).toInt() > 0;
    query.finish();
    return exists;
}

QJsonArray rowsFromQuery(QSqlQuery* query)
{
    QJsonArray rows;
    while (query->next()) {
        QJsonObject row;
        const auto record = query->record();
        for (int i = 0; i < record.count(); ++i) {
            row[record.fieldName(i)] = QJsonValue::fromVariant(query->value(i));
        }
        rows.append(row);
    }
    return rows;
}

QDate payloadDateOrDefault(const QJsonObject& payload, const QString& key, const QDate& fallback)
{
    const QDate date = QDate::fromString(payload.value(key).toString(), Qt::ISODate);
    return date.isValid() ? date : fallback;
}

void normalizeDateRange(const QJsonObject& payload, QDate* startDate, QDate* endDate)
{
    const QDate today = QDate::currentDate();
    *startDate = payloadDateOrDefault(payload, QStringLiteral("startDate"), today);
    *endDate = payloadDateOrDefault(payload, QStringLiteral("endDate"), *startDate);
    if (*endDate < *startDate) {
        std::swap(*startDate, *endDate);
    }
}

QJsonArray rowsFromDateRangeQuery(DatabaseManager* database, const QString& sql, const QDate& startDate, const QDate& endDate)
{
    QSqlQuery query(database->database());
    query.prepare(sql);
    query.bindValue(":start_date", startDate);
    query.bindValue(":end_date", endDate);
    if (!query.exec()) {
        return {};
    }
    auto rows = rowsFromQuery(&query);
    query.finish();
    return rows;
}

QJsonArray demoDailyVisits()
{
    const QDate today = QDate::currentDate();
    return QJsonArray{
        QJsonObject{{"统计日期", today.addDays(-6).toString(Qt::ISODate)}, {"门诊量", 28}},
        QJsonObject{{"统计日期", today.addDays(-5).toString(Qt::ISODate)}, {"门诊量", 35}},
        QJsonObject{{"统计日期", today.addDays(-4).toString(Qt::ISODate)}, {"门诊量", 31}},
        QJsonObject{{"统计日期", today.addDays(-3).toString(Qt::ISODate)}, {"门诊量", 42}},
        QJsonObject{{"统计日期", today.addDays(-2).toString(Qt::ISODate)}, {"门诊量", 39}},
        QJsonObject{{"统计日期", today.addDays(-1).toString(Qt::ISODate)}, {"门诊量", 45}},
        QJsonObject{{"统计日期", today.toString(Qt::ISODate)}, {"门诊量", 37}}
    };
}

QJsonArray demoDepartmentVisits()
{
    return QJsonArray{
        QJsonObject{{"科室", "心血管内科"}, {"接诊量", 18}},
        QJsonObject{{"科室", "普外科"}, {"接诊量", 14}},
        QJsonObject{{"科室", "呼吸内科"}, {"接诊量", 12}},
        QJsonObject{{"科室", "儿科"}, {"接诊量", 9}},
        QJsonObject{{"科室", "骨科"}, {"接诊量", 8}}
    };
}

QJsonArray demoDoctorRanking()
{
    return QJsonArray{
        QJsonObject{{"医生", "李华"}, {"接诊量", 16}},
        QJsonObject{{"医生", "马骏胜"}, {"接诊量", 13}},
        QJsonObject{{"医生", "张明"}, {"接诊量", 10}},
        QJsonObject{{"医生", "王敏"}, {"接诊量", 8}},
        QJsonObject{{"医生", "赵强"}, {"接诊量", 6}}
    };
}

common::Response demoDashboard(const QJsonObject& payload)
{
    const auto registrations = DemoRepository::instance().rows("registrations", {});
    const auto inventory = DemoRepository::instance().rows("inventory", {});
    const auto statistics = DemoRepository::instance().rows("statistics", {});
    QDate startDate;
    QDate endDate;
    normalizeDateRange(payload, &startDate, &endDate);
    QJsonObject data;
    data["todayRegistrations"] = registrations.size();
    data["waitingPatients"] = DemoRepository::instance().waitingQueue({}).size();
    data["finishedPatients"] = 1;
    double totalIncome = 0;
    for (const auto& item : statistics) {
        const auto row = item.toObject();
        if (row.value("科室").toString() == "全院") {
            totalIncome = row.value("总收入").toVariant().toDouble();
            data["registrationIncome"] = row.value("挂号收入").toVariant().toDouble();
            data["drugIncome"] = row.value("药品收入").toVariant().toDouble();
        }
    }
    data["todayIncome"] = totalIncome;
    data["doctorTop"] = QJsonArray{
        QJsonObject{{"医生", "李华"}, {"接诊量", 1}},
        QJsonObject{{"医生", "张明"}, {"接诊量", 0}}
    };

    QJsonArray warnings;
    for (const auto& item : inventory) {
        const auto drug = item.toObject();
        if (drug.value("库存").toVariant().toInt() <= drug.value("预警库存").toVariant().toInt()) {
            warnings.append(drug);
        }
    }
    data["stockWarnings"] = warnings;
    data["startDate"] = startDate.toString(Qt::ISODate);
    data["endDate"] = endDate.toString(Qt::ISODate);
    data["dailyVisits"] = demoDailyVisits();
    data["departmentVisits"] = demoDepartmentVisits();
    data["doctorRanking"] = demoDoctorRanking();
    return {true, "OK", data};
}

common::Response dashboardStats(DatabaseManager* database)
{
    const bool hasAppointments = tableExists(database, QStringLiteral("appointments"));

    QJsonObject data;
    if (hasAppointments) {
        data["today_registrations"] = scalarOrDefault(
            database,
            "SELECT COUNT(*) FROM appointments WHERE DATE(created_at) = CURRENT_DATE",
            0);
        data["current_waiting"] = scalarOrDefault(
            database,
            "SELECT COUNT(*) FROM appointments WHERE status = 'WAITING'",
            0);
    } else {
        data["today_registrations"] = scalarOrDefault(
            database,
            "SELECT COUNT(*) FROM registrations WHERE DATE(register_time) = CURRENT_DATE",
            0);
        data["current_waiting"] = scalarOrDefault(
            database,
            "SELECT COUNT(*) FROM registrations WHERE status IN ('WAITING', 'CALLED', 'CHECK_DONE')",
            0);
    }

    data["revenue"] = scalarOrDefault(
        database,
        "SELECT COALESCE(SUM(amount), 0) FROM payments WHERE DATE(pay_time) = CURRENT_DATE",
        0);

    return {true, "OK", data};
}

common::Response dashboardTopDoctors(DatabaseManager* database)
{
    const bool hasAppointments = tableExists(database, QStringLiteral("appointments"));

    QSqlQuery query(database->database());
    if (hasAppointments) {
        query.prepare("SELECT u.real_name AS doctor, COUNT(*) AS visits "
                      "FROM appointments a "
                      "JOIN doctor_schedules s ON s.id = CAST(a.schedule_id AS UNSIGNED) "
                      "JOIN doctors doc ON doc.id = s.doctor_id "
                      "JOIN users u ON u.id = doc.user_id "
                      "WHERE DATE(a.created_at) = CURRENT_DATE "
                      "GROUP BY u.real_name "
                      "ORDER BY COUNT(*) DESC LIMIT 5");
    } else {
        query.prepare("SELECT u.real_name AS doctor, COUNT(*) AS visits "
                      "FROM registrations r "
                      "JOIN doctors doc ON doc.id = r.doctor_id "
                      "JOIN users u ON u.id = doc.user_id "
                      "WHERE r.status = 'FINISHED' AND DATE(r.register_time) = CURRENT_DATE "
                      "GROUP BY u.real_name ORDER BY COUNT(*) DESC LIMIT 5");
    }

    QJsonObject data;
    if (query.exec()) {
        data["rows"] = rowsFromQuery(&query);
        query.finish();
    } else {
        data["rows"] = QJsonArray{};
    }
    return {true, "OK", data};
}

} // namespace

DashboardService::DashboardService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response DashboardService::handle(const common::Request& request)
{
    if (request.action != "summary" && request.action != "stats" && request.action != "topDoctors") {
        return {false, "Unsupported dashboard action", {}};
    }

    if (!m_database->isEnabled()) {
        return demoDashboard(request.payload);
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    if (request.action == "stats") {
        return dashboardStats(m_database);
    }

    if (request.action == "topDoctors") {
        return dashboardTopDoctors(m_database);
    }

    QDate startDate;
    QDate endDate;
    normalizeDateRange(request.payload, &startDate, &endDate);

    QJsonObject data;
    data["startDate"] = startDate.toString(Qt::ISODate);
    data["endDate"] = endDate.toString(Qt::ISODate);
    QJsonValue value;
    if (scalarBound(m_database,
            "SELECT COUNT(*) FROM registrations WHERE DATE(register_time) BETWEEN :start_date AND :end_date",
            startDate, endDate, &value)) {
        data["todayRegistrations"] = value;
    }
    if (scalar(m_database, "SELECT COUNT(*) FROM registrations WHERE status IN ('WAITING', 'CALLED', 'CHECK_DONE')", &value)) {
        data["waitingPatients"] = value;
    }
    if (scalarBound(m_database,
            "SELECT COUNT(*) FROM registrations WHERE status = 'FINISHED' AND DATE(register_time) BETWEEN :start_date AND :end_date",
            startDate, endDate, &value)) {
        data["finishedPatients"] = value;
    }
    if (scalarBound(m_database,
            "SELECT COALESCE(SUM(total_amount), 0) FROM bills WHERE status = 'PAID' AND DATE(created_at) BETWEEN :start_date AND :end_date",
            startDate, endDate, &value)) {
        data["todayIncome"] = value;
    }
    if (scalarBound(m_database,
            "SELECT COALESCE(SUM(registration_fee), 0) FROM bills WHERE status = 'PAID' AND DATE(created_at) BETWEEN :start_date AND :end_date",
            startDate, endDate, &value)) {
        data["registrationIncome"] = value;
    }
    if (scalarBound(m_database,
            "SELECT COALESCE(SUM(drug_fee), 0) FROM bills WHERE status = 'PAID' AND DATE(created_at) BETWEEN :start_date AND :end_date",
            startDate, endDate, &value)) {
        data["drugIncome"] = value;
    }

    data["dailyVisits"] = rowsFromDateRangeQuery(m_database,
        "SELECT DATE(r.register_time) AS '统计日期', COUNT(*) AS '门诊量' "
        "FROM registrations r "
        "WHERE DATE(r.register_time) BETWEEN :start_date AND :end_date "
        "GROUP BY DATE(r.register_time) ORDER BY DATE(r.register_time)",
        startDate, endDate);

    data["departmentVisits"] = rowsFromDateRangeQuery(m_database,
        "SELECT d.dept_name AS '科室', COUNT(*) AS '接诊量' "
        "FROM registrations r "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN departments d ON d.id = doc.department_id "
        "WHERE r.status = 'FINISHED' AND DATE(r.register_time) BETWEEN :start_date AND :end_date "
        "GROUP BY d.dept_name ORDER BY COUNT(*) DESC LIMIT 10",
        startDate, endDate);

    data["doctorRanking"] = rowsFromDateRangeQuery(m_database,
        "SELECT u.real_name AS '医生', COUNT(*) AS '接诊量' "
        "FROM registrations r "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "WHERE r.status = 'FINISHED' AND DATE(r.register_time) BETWEEN :start_date AND :end_date "
        "GROUP BY u.real_name ORDER BY COUNT(*) DESC LIMIT 10",
        startDate, endDate);
    data["doctorTop"] = data.value("doctorRanking").toArray();

    {
        QSqlQuery query(m_database->database());
        query.prepare("SELECT drug_code AS '药品编码', drug_name AS '药品名称', stock_quantity AS '库存', "
                      "warning_quantity AS '预警库存' "
                      "FROM drugs WHERE status = 1 AND stock_quantity <= warning_quantity "
                      "ORDER BY stock_quantity ASC LIMIT 10");
        if (query.exec()) {
            data["stockWarnings"] = rowsFromQuery(&query);
            query.finish();
        }
    }

    return {true, "OK", data};
}

} // namespace hospital::server
