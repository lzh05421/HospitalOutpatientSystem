#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/ScheduleRuleEngine.h"
#include "server/SqlJson.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace hospital::server {
namespace {

QStringList& demoScheduleRuleTexts()
{
    static QStringList rules;
    return rules;
}

struct ScheduleSaveContext
{
    qint64 doctorId = 0;
    qint64 departmentId = 0;
    QString doctorName;
    QString departmentName;
    QString title;
    QDate workDate;
};

QDate dateFromSqlValue(const QVariant& value)
{
    const QDate date = value.toDate();
    if (date.isValid()) {
        return date;
    }
    return QDate::fromString(value.toString().left(10), "yyyy-MM-dd");
}

qint64 departmentIdByName(QSqlDatabase& database, const QString& departmentName)
{
    QSqlQuery query(database);
    query.prepare("SELECT id FROM departments WHERE dept_name = :dept LIMIT 1");
    query.bindValue(":dept", departmentName.trimmed());
    if (query.exec() && query.next()) {
        const qint64 id = query.value(0).toLongLong();
        query.finish();
        return id;
    }
    query.finish();
    return 0;
}

QVector<ScheduleRule> loadActiveScheduleRules(QSqlDatabase& database)
{
    QVector<ScheduleRule> rules;
    QSqlQuery query(database);
    query.prepare("SELECT rule_type, target_type, doctor_id, department_id, title, target_text, "
                  "date_mode, weekdays_mask, start_date, end_date, reason, raw_text "
                  "FROM schedule_rules WHERE enabled = 1");
    if (!query.exec()) {
        return rules;
    }

    while (query.next()) {
        ScheduleRule rule;
        rule.ruleType = query.value(0).toString();
        rule.targetType = query.value(1).toString();
        rule.doctorId = query.value(2).toLongLong();
        rule.departmentId = query.value(3).toLongLong();
        rule.title = query.value(4).toString();
        rule.targetText = query.value(5).toString();
        rule.dateMode = query.value(6).toString();
        rule.weekdaysMask = query.value(7).toInt();
        rule.startDate = dateFromSqlValue(query.value(8));
        rule.endDate = dateFromSqlValue(query.value(9));
        rule.reason = query.value(10).toString();
        rule.rawText = query.value(11).toString();
        rules.append(rule);
    }
    query.finish();
    return rules;
}

common::Response validateScheduleRules(QSqlDatabase& database, const ScheduleSaveContext& context)
{
    ScheduleRuleMatchContext matchContext;
    matchContext.doctorId = context.doctorId;
    matchContext.departmentId = context.departmentId;
    matchContext.doctorName = context.doctorName;
    matchContext.departmentName = context.departmentName;
    matchContext.title = context.title;
    matchContext.workDate = context.workDate;

    const ScheduleRuleValidationResult result = ScheduleRuleEngine::validate(loadActiveScheduleRules(database), matchContext);
    if (!result.allowed) {
        return {false, result.message, {}};
    }
    return {true, "OK", {}};
}

ScheduleRule ruleFromParsedText(const ScheduleRuleParseResult& parsed)
{
    ScheduleRule rule;
    rule.ruleType = parsed.ruleType;
    rule.targetText = parsed.targetText;
    rule.dateMode = parsed.dateMode;
    rule.weekdaysMask = parsed.weekdaysMask;
    rule.startDate = parsed.startDate;
    rule.endDate = parsed.endDate;
    rule.reason = parsed.reason;
    return rule;
}

common::Response resolveRuleTarget(QSqlDatabase& database, ScheduleRule* rule)
{
    const QString target = rule->targetText.trimmed();
    if (target.isEmpty() || target == "全部" || target == "全部医生") {
        rule->targetType = "ALL";
        return {true, "OK", {}};
    }

    {
        QSqlQuery query(database);
        query.prepare("SELECT doc.id FROM doctors doc "
                      "JOIN users u ON u.id = doc.user_id "
                      "WHERE u.real_name = :target LIMIT 1");
        query.bindValue(":target", target);
        if (!query.exec()) {
            return {false, query.lastError().text(), {}};
        }
        if (query.next()) {
            rule->targetType = "DOCTOR";
            rule->doctorId = query.value(0).toLongLong();
            query.finish();
            return {true, "OK", {}};
        }
        query.finish();
    }

    {
        QSqlQuery query(database);
        query.prepare("SELECT id FROM departments WHERE dept_name = :target LIMIT 1");
        query.bindValue(":target", target);
        if (!query.exec()) {
            return {false, query.lastError().text(), {}};
        }
        if (query.next()) {
            rule->targetType = "DEPARTMENT";
            rule->departmentId = query.value(0).toLongLong();
            query.finish();
            return {true, "OK", {}};
        }
        query.finish();
    }

    rule->targetType = "TITLE";
    rule->title = target;
    return {true, "OK", {}};
}

QVector<ScheduleRule> demoRulesFromTexts(const QStringList& texts)
{
    QVector<ScheduleRule> rules;
    for (const QString& text : texts) {
        const auto parsed = ScheduleRuleEngine::parseFixedRuleText(text);
        if (!parsed.success) {
            continue;
        }
        ScheduleRule rule = ruleFromParsedText(parsed);
        const QString target = rule.targetText.trimmed();
        if (target.isEmpty() || target == "全部" || target == "全部医生") {
            rule.targetType = "ALL";
        } else if (target.contains("主任") || target.contains("医师") || target.contains("专家")) {
            rule.targetType = "TITLE";
            rule.title = target;
        } else {
            rule.targetType = "DOCTOR";
        }
        rule.rawText = text;
        rules.append(rule);
    }
    return rules;
}

common::Response validateDemoScheduleRules(const QJsonObject& payload)
{
    ScheduleRuleMatchContext context;
    context.doctorName = payload.value("doctor").toString().trimmed();
    context.departmentName = payload.value("department").toString().trimmed();
    context.title = payload.value("title").toString().trimmed();
    context.workDate = QDate::fromString(payload.value("date").toString().trimmed(), "yyyy-MM-dd");

    const auto result = ScheduleRuleEngine::validate(demoRulesFromTexts(demoScheduleRuleTexts()), context);
    if (!result.allowed) {
        return {false, result.message, {}};
    }
    return {true, "OK", {}};
}

QJsonArray ruleTextsToJsonArray(const QStringList& rules)
{
    QJsonArray array;
    for (const QString& rule : rules) {
        array.append(rule);
    }
    return array;
}

common::Response listScheduleRulesInDatabase(DatabaseManager* database)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QJsonArray rules;
    QSqlQuery query(database->database());
    query.prepare("SELECT raw_text FROM schedule_rules WHERE enabled = 1 ORDER BY id");
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    while (query.next()) {
        const QString text = query.value(0).toString().trimmed();
        if (!text.isEmpty()) {
            rules.append(text);
        }
    }
    query.finish();

    QJsonObject data;
    data["module"] = "schedule";
    data["action"] = "rulesList";
    data["rules"] = rules;
    return {true, "长期排班规则已读取。", data};
}

common::Response listScheduleRangeInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QDate startDate = QDate::fromString(payload.value("startDate").toString().trimmed(), "yyyy-MM-dd");
    const QDate endDate = QDate::fromString(payload.value("endDate").toString().trimmed(), "yyyy-MM-dd");
    if (!startDate.isValid() || !endDate.isValid() || startDate > endDate) {
        return {false, "排班日期范围无效。", {}};
    }

    QJsonArray rows;
    QSqlQuery query(database->database());
    query.prepare("SELECT d.dept_name AS department, u.real_name AS doctor, doc.title AS title, "
                  "s.work_date AS work_date, s.period AS period, s.total_quota AS total_quota, "
                  "s.remain_quota AS remain_quota, s.status AS status "
                  "FROM doctor_schedules s "
                  "JOIN doctors doc ON doc.id = s.doctor_id "
                  "JOIN users u ON u.id = doc.user_id "
                  "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
                  "WHERE s.work_date BETWEEN :start_date AND :end_date "
                  "AND doc.status = 1 AND u.status = 1 "
                  "ORDER BY s.work_date, u.real_name, s.period");
    query.bindValue(":start_date", startDate.toString("yyyy-MM-dd"));
    query.bindValue(":end_date", endDate.toString("yyyy-MM-dd"));
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    while (query.next()) {
        QJsonObject row;
        row["科室"] = query.value(0).toString();
        row["医生"] = query.value(1).toString();
        row["职称"] = query.value(2).toString();
        row["出诊日期"] = dateFromSqlValue(query.value(3)).toString("yyyy-MM-dd");
        row["时段"] = query.value(4).toString();
        row["总号源"] = query.value(5).toInt();
        row["剩余号源"] = query.value(6).toInt();
        row["状态"] = query.value(7).toInt();
        rows.append(row);
    }
    query.finish();

    QJsonObject data;
    data["module"] = "schedule";
    data["action"] = "rangeList";
    data["rows"] = rows;
    data["startDate"] = startDate.toString("yyyy-MM-dd");
    data["endDate"] = endDate.toString("yyyy-MM-dd");
    return {true, QString("已读取 %1 条范围排班。").arg(rows.size()), data};
}

common::Response listScheduleRangeInDemo(const QJsonObject& payload)
{
    const QDate startDate = QDate::fromString(payload.value("startDate").toString().trimmed(), "yyyy-MM-dd");
    const QDate endDate = QDate::fromString(payload.value("endDate").toString().trimmed(), "yyyy-MM-dd");
    if (!startDate.isValid() || !endDate.isValid() || startDate > endDate) {
        return {false, "排班日期范围无效。", {}};
    }

    QJsonArray rows;
    const QJsonArray schedules = DemoRepository::instance().rows("schedules");
    for (const auto& item : schedules) {
        const auto row = item.toObject();
        const QDate date = QDate::fromString(row.value("出诊日期").toString(), "yyyy-MM-dd");
        if (date.isValid() && date >= startDate && date <= endDate) {
            rows.append(row);
        }
    }

    QJsonObject data;
    data["module"] = "schedule";
    data["action"] = "rangeList";
    data["rows"] = rows;
    data["startDate"] = startDate.toString("yyyy-MM-dd");
    data["endDate"] = endDate.toString("yyyy-MM-dd");
    return {true, QString("已读取 %1 条范围排班。").arg(rows.size()), data};
}

common::Response saveAllScheduleRulesInDatabase(DatabaseManager* database, const QJsonArray& inputRules, qint64 operatorUserId)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlDatabase db = database->database();
    if (!db.transaction()) {
        return {false, "开启规则保存事务失败：" + db.lastError().text(), {}};
    }

    QSqlQuery disable(db);
    if (!disable.exec("UPDATE schedule_rules SET enabled = 0")) {
        const QString error = disable.lastError().text();
        db.rollback();
        return {false, error, {}};
    }
    disable.finish();

    int saved = 0;
    const QString codePrefix = "SR" + QString::number(QDateTime::currentMSecsSinceEpoch());
    for (const auto& item : inputRules) {
        const QString rawText = item.toString().trimmed();
        if (rawText.isEmpty()) {
            continue;
        }

        const auto parsed = ScheduleRuleEngine::parseFixedRuleText(rawText);
        if (!parsed.success) {
            db.rollback();
            return {false, QString("第 %1 条长期规则格式错误：%2").arg(saved + 1).arg(parsed.message), {}};
        }

        ScheduleRule rule = ruleFromParsedText(parsed);
        rule.rawText = rawText;
        const auto targetResult = resolveRuleTarget(db, &rule);
        if (!targetResult.success) {
            db.rollback();
            return targetResult;
        }

        QSqlQuery insert(db);
        insert.prepare("INSERT INTO schedule_rules "
                       "(rule_code, rule_type, target_type, doctor_id, department_id, title, target_text, "
                       "date_mode, weekdays_mask, start_date, end_date, reason, raw_text, enabled, created_by) "
                       "VALUES (:rule_code, :rule_type, :target_type, :doctor_id, :department_id, :title, :target_text, "
                       ":date_mode, :weekdays_mask, :start_date, :end_date, :reason, :raw_text, 1, :created_by)");
        insert.bindValue(":rule_code", codePrefix + QString::number(saved + 1).rightJustified(2, '0'));
        insert.bindValue(":rule_type", rule.ruleType);
        insert.bindValue(":target_type", rule.targetType);
        insert.bindValue(":doctor_id", rule.doctorId > 0 ? QVariant(rule.doctorId) : QVariant());
        insert.bindValue(":department_id", rule.departmentId > 0 ? QVariant(rule.departmentId) : QVariant());
        insert.bindValue(":title", rule.title.trimmed().isEmpty() ? QVariant() : QVariant(rule.title.trimmed()));
        insert.bindValue(":target_text", rule.targetText.trimmed().isEmpty() ? QVariant() : QVariant(rule.targetText.trimmed()));
        insert.bindValue(":date_mode", rule.dateMode);
        insert.bindValue(":weekdays_mask", rule.weekdaysMask);
        insert.bindValue(":start_date", rule.startDate.isValid() ? QVariant(rule.startDate) : QVariant());
        insert.bindValue(":end_date", rule.endDate.isValid() ? QVariant(rule.endDate) : QVariant());
        insert.bindValue(":reason", rule.reason.trimmed().isEmpty() ? QVariant() : QVariant(rule.reason.trimmed()));
        insert.bindValue(":raw_text", rawText);
        insert.bindValue(":created_by", operatorUserId > 0 ? QVariant(operatorUserId) : QVariant());
        if (!insert.exec()) {
            const QString error = insert.lastError().text();
            db.rollback();
            return {false, error, {}};
        }
        insert.finish();
        ++saved;
    }

    if (!db.commit()) {
        const QString error = db.lastError().text();
        db.rollback();
        return {false, "提交长期规则失败：" + error, {}};
    }

    QJsonObject data;
    data["module"] = "schedule";
    data["action"] = "rulesSaveAll";
    data["saved"] = saved;
    return {true, QString("已保存 %1 条长期排班规则。").arg(saved), data};
}

common::Response ensureDepartment(DatabaseManager* database, const QString& departmentName, qint64* departmentId)
{
    if (departmentName.trimmed().isEmpty()) {
        return {false, "诊室不能为空。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT id FROM departments WHERE dept_name = :dept LIMIT 1");
    query.bindValue(":dept", departmentName.trimmed());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    if (query.next()) {
        *departmentId = query.value(0).toLongLong();
        query.finish();
        return {true, "OK", {}};
    }
    query.finish();

    query.prepare("INSERT INTO departments (dept_code, dept_name, location, status) "
                  "VALUES (:dept_code, :dept_name, '', 1)");
    query.bindValue(":dept_code", "DEPCL" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.bindValue(":dept_name", departmentName.trimmed());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    *departmentId = query.lastInsertId().toLongLong();
    return {true, "新诊室已同步保存。", {}};
}

common::Response saveScheduleInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();

    const QString doctor = payload.value("doctor").toString().trimmed();
    const QString department = payload.value("department").toString().trimmed();
    const QString date = payload.value("date").toString().trimmed();
    const QString period = "全天";
    const int quota = payload.value("quota").toInt(30);
    const QDate workDate = QDate::fromString(date, "yyyy-MM-dd");

    if (doctor.isEmpty() || department.isEmpty() || !workDate.isValid()) {
        return {false, "医生、诊室和出诊日期不能为空。", {}};
    }
    if (workDate < QDate::currentDate()) {
        return {false, "不能为过去日期排班。", {}};
    }
    if (quota <= 0) {
        return {false, "号源数量必须大于 0。", {}};
    }

    ScheduleSaveContext context;
    context.doctorName = doctor;
    context.departmentName = department;
    context.workDate = workDate;
    {
        QSqlQuery query(db);
        query.prepare("SELECT doc.id, doc.title FROM doctors doc "
                      "JOIN users u ON u.id = doc.user_id "
                      "WHERE u.real_name = :doctor LIMIT 1");
        query.bindValue(":doctor", doctor);
        if (!query.exec() || !query.next()) {
            return {false, "未找到可用医生。", {}};
        }
        context.doctorId = query.value(0).toLongLong();
        context.title = query.value(1).toString();
        if (context.title.trimmed().isEmpty()) {
            context.title = payload.value("title").toString();
        }
        query.finish();
    }

    context.departmentId = departmentIdByName(db, department);
    const auto ruleResult = validateScheduleRules(db, context);
    if (!ruleResult.success) {
        return ruleResult;
    }

    {
        QSqlQuery restore(db);
        restore.prepare("UPDATE doctors doc JOIN users u ON u.id = doc.user_id "
                        "SET doc.status = 1, u.status = 1 WHERE doc.id = :doctor_id");
        restore.bindValue(":doctor_id", context.doctorId);
        if (!restore.exec()) {
            return {false, restore.lastError().text(), {}};
        }
        restore.finish();
    }

    qint64 departmentId = 0;
    if (!department.isEmpty()) {
        const auto deptResult = ensureDepartment(database, department, &departmentId);
        if (!deptResult.success) {
            return deptResult;
        }
    }

    bool scheduleExists = false;
    int used = 0;
    {
        QSqlQuery query(db);
        query.prepare("SELECT period, total_quota, remain_quota, status FROM doctor_schedules "
                      "WHERE doctor_id = :doctor_id AND work_date = :work_date");
        query.bindValue(":doctor_id", context.doctorId);
        query.bindValue(":work_date", date);
        if (!query.exec()) {
            return {false, query.lastError().text(), {}};
        }

        while (query.next()) {
            scheduleExists = scheduleExists || query.value(0).toString() == period;
            if (query.value(3).toInt() == 1) {
                used += query.value(1).toInt() - query.value(2).toInt();
            }
        }
        query.finish();
    }

    if (scheduleExists) {
        if (quota < used) {
            return {false, QString("该排班已预约 %1 人，号源数量不能小于已预约人数。").arg(used), {}};
        }
        QSqlQuery update(db);
        update.prepare("UPDATE doctor_schedules "
                       "SET department_id = :department_id, total_quota = :quota, remain_quota = :remain_quota, status = 1 "
                       "WHERE doctor_id = :doctor_id AND work_date = :work_date AND period = :period");
        update.bindValue(":department_id", departmentId > 0 ? QVariant(departmentId) : QVariant());
        update.bindValue(":quota", quota);
        update.bindValue(":remain_quota", qMax(0, quota - used));
        update.bindValue(":doctor_id", context.doctorId);
        update.bindValue(":work_date", date);
        update.bindValue(":period", period);
        if (!update.exec()) {
            return {false, update.lastError().text(), {}};
        }
    } else {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO doctor_schedules (doctor_id, department_id, work_date, period, total_quota, remain_quota, status) "
                       "VALUES (:doctor_id, :department_id, :work_date, :period, :total_quota, :remain_quota, 1)");
        insert.bindValue(":doctor_id", context.doctorId);
        insert.bindValue(":department_id", departmentId > 0 ? QVariant(departmentId) : QVariant());
        insert.bindValue(":work_date", date);
        insert.bindValue(":period", period);
        insert.bindValue(":total_quota", quota);
        insert.bindValue(":remain_quota", quota);
        if (!insert.exec()) {
            return {false, insert.lastError().text(), {}};
        }
    }

    QSqlQuery disableOldPeriods(db);
    disableOldPeriods.prepare("UPDATE doctor_schedules "
                              "SET status = 0, remain_quota = 0 "
                              "WHERE doctor_id = :doctor_id AND work_date = :work_date AND period <> :period");
    disableOldPeriods.bindValue(":doctor_id", context.doctorId);
    disableOldPeriods.bindValue(":work_date", date);
    disableOldPeriods.bindValue(":period", period);
    if (!disableOldPeriods.exec()) {
        return {false, disableOldPeriods.lastError().text(), {}};
    }

    return {true, "排班号源已写入 MySQL。", {}};
}

common::Response updateScheduleInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    QJsonObject mapped;
    mapped["doctor"] = payload.value("医生").toString();
    mapped["department"] = payload.value("科室").toString();
    mapped["title"] = payload.value("职称").toString();
    mapped["date"] = payload.value("出诊日期").toString();
    mapped["period"] = "全天";
    mapped["quota"] = payload.value("总号源").toVariant().toInt();
    return saveScheduleInDatabase(database, mapped);
}

common::Response disableScheduleInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE doctor_schedules s "
                  "JOIN doctors doc ON doc.id = s.doctor_id "
                  "JOIN users u ON u.id = doc.user_id "
                  "SET s.status = 0, s.remain_quota = 0 "
                  "WHERE u.real_name = :doctor AND s.work_date = :work_date");
    query.bindValue(":doctor", payload.value("医生").toString());
    query.bindValue(":work_date", payload.value("出诊日期").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "排班已停诊。", {}};
}

common::Response resetSchedulesInDatabase(DatabaseManager* database)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE doctor_schedules SET status = 0, remain_quota = 0 WHERE status = 1");
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    QJsonObject data;
    data["resetCount"] = query.numRowsAffected();
    return {true, QString("已清空 %1 条排班数据，可以重新排班。").arg(query.numRowsAffected()), data};
}

} // namespace

ScheduleService::ScheduleService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response ScheduleService::handle(const common::Request& request)
{
    if (request.action == "rangeList") {
        if (m_database->isEnabled()) {
            return listScheduleRangeInDatabase(m_database, request.payload);
        }
        return listScheduleRangeInDemo(request.payload);
    }

    if (request.action == "rulesList") {
        if (m_database->isEnabled()) {
            return listScheduleRulesInDatabase(m_database);
        }

        QJsonObject data;
        data["module"] = "schedule";
        data["action"] = "rulesList";
        data["rules"] = ruleTextsToJsonArray(demoScheduleRuleTexts());
        return {true, "长期排班规则已读取。", data};
    }

    if (request.action == "rulesSaveAll") {
        const QJsonArray rules = request.payload.value("rules").toArray();
        if (m_database->isEnabled()) {
            const qint64 operatorUserId = request.payload.value("__operatorUserId").toVariant().toLongLong();
            return saveAllScheduleRulesInDatabase(m_database, rules, operatorUserId);
        }

        QStringList texts;
        int index = 0;
        for (const auto& item : rules) {
            ++index;
            const QString text = item.toString().trimmed();
            if (text.isEmpty()) {
                continue;
            }
            const auto parsed = ScheduleRuleEngine::parseFixedRuleText(text);
            if (!parsed.success) {
                return {false, QString("第 %1 条长期规则格式错误：%2").arg(index).arg(parsed.message), {}};
            }
            texts.append(text);
        }
        demoScheduleRuleTexts() = texts;

        QJsonObject data;
        data["module"] = "schedule";
        data["action"] = "rulesSaveAll";
        data["saved"] = texts.size();
        return {true, QString("已保存 %1 条长期排班规则。").arg(texts.size()), data};
    }

    if (request.action == "save") {
        if (m_database->isEnabled()) {
            return saveScheduleInDatabase(m_database, request.payload);
        }

        const auto ruleResult = validateDemoScheduleRules(request.payload);
        if (!ruleResult.success) {
            return ruleResult;
        }
        const auto result = DemoRepository::instance().saveSchedule(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }
    if (request.action == "batchSave") {
        const QJsonArray rows = request.payload.value("rows").toArray();
        int saved = 0;
        if (m_database->isEnabled()) {
            if (!m_database->ensureOpen()) {
                return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
            }

            QSqlDatabase db = m_database->database();
            if (!db.transaction()) {
                return {false, "开启排班事务失败：" + db.lastError().text(), {}};
            }

            for (const auto& item : rows) {
                const QJsonObject row = item.toObject();
                const common::Response response = saveScheduleInDatabase(m_database, row);
                if (!response.success) {
                    db.rollback();
                    QJsonObject data;
                    data["rolledBack"] = true;
                    data["failedIndex"] = saved + 1;
                    return {
                        false,
                        QString("智能排班失败，已回滚所有排班。第 %1 条失败：%2")
                            .arg(saved + 1)
                            .arg(response.message),
                        data
                    };
                }
                ++saved;
            }

            if (!db.commit()) {
                const QString error = db.lastError().text();
                db.rollback();
                return {false, "提交排班事务失败：" + error, {}};
            }

            QJsonObject data;
            data["module"] = "schedule";
            data["action"] = "batchSave";
            data["saved"] = saved;
            data["transactional"] = true;
            return {true, QString("智能排班已生成 %1 条排班。").arg(saved), data};
        }

        for (const auto& item : rows) {
            const QJsonObject row = item.toObject();
            const auto ruleResult = validateDemoScheduleRules(row);
            if (!ruleResult.success) {
                return ruleResult;
            }
            const auto result = DemoRepository::instance().saveSchedule(row);
            common::Response response = {result.value("success").toBool(), result.value("message").toString(), result};
            if (!response.success) {
                return response;
            }
            ++saved;
        }
        QJsonObject data;
        data["module"] = "schedule";
        data["action"] = "batchSave";
        data["saved"] = saved;
        return {true, QString("智能排班已生成 %1 条排班。").arg(saved), data};
    }
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            QJsonObject mapped;
            mapped["doctor"] = request.payload.value("医生").toString();
            mapped["department"] = request.payload.value("科室").toString();
            mapped["title"] = request.payload.value("职称").toString();
            mapped["date"] = request.payload.value("出诊日期").toString();
            mapped["period"] = "全天";
            mapped["quota"] = request.payload.value("总号源").toVariant().toInt();
            const auto result = DemoRepository::instance().saveSchedule(mapped);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateScheduleInDatabase(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().disableSchedule(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return disableScheduleInDatabase(m_database, request.payload);
    }
    if (request.action == "reset") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().resetSchedules();
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return resetSchedulesInDatabase(m_database);
    }

    if (request.action != "list") {
        return {false, "Unsupported schedule action", {}};
    }

    // 智能排班从用户选择的开始日期优先展示，避免 6 月 17 日生成后列表先显示 6 月 21 日。
    return SqlJson::selectRows(m_database,
        "SELECT d.dept_name AS '科室', u.real_name AS '医生', doc.title AS '职称', "
        "s.work_date AS '出诊日期', SUM(s.total_quota) AS '总号源', "
        "SUM(s.remain_quota) AS '剩余号源', 1 AS '状态' "
        "FROM doctor_schedules s "
        "JOIN doctors doc ON doc.id = s.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        "WHERE s.status = 1 AND doc.status = 1 AND u.status = 1 "
        "GROUP BY d.dept_name, u.real_name, doc.title, s.work_date "
        "ORDER BY s.work_date ASC, d.dept_name, u.real_name LIMIT 100",
        {}, "schedules");
}

} // namespace hospital::server
