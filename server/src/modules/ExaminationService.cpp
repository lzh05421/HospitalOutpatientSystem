#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"
#include "server/WorkflowRules.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

namespace hospital::server {
namespace {

common::Response listExaminationItemsInDatabase(DatabaseManager* database)
{
    return SqlJson::selectRows(database,
        "SELECT item_code AS '项目编码', item_name AS '检查项目', category AS '项目分类', "
        "unit_price AS '单价', CASE status WHEN 1 THEN '启用' ELSE '停用' END AS '状态' "
        "FROM examination_items ORDER BY status DESC, category, item_name LIMIT 300",
        {}, "examinationItems");
}

common::Response saveExaminationItemInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString itemCode = payload.value("项目编码").toString(payload.value("itemCode").toString()).trimmed();
    const QString itemName = payload.value("检查项目").toString(payload.value("itemName").toString()).trimmed();
    const QString category = payload.value("项目分类").toString(payload.value("category").toString("检查")).trimmed();
    const double unitPrice = payload.value("单价").toVariant().toDouble();
    if (itemCode.isEmpty() || itemName.isEmpty()) {
        return {false, "项目编码和检查项目不能为空。", {}};
    }
    if (unitPrice < 0.0) {
        return {false, "检查项目单价不能小于 0。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("INSERT INTO examination_items (item_code, item_name, category, unit_price, status) "
                  "VALUES (:item_code, :item_name, :category, :unit_price, 1) "
                  "ON DUPLICATE KEY UPDATE item_name = VALUES(item_name), category = VALUES(category), "
                  "unit_price = VALUES(unit_price), status = 1");
    query.bindValue(":item_code", itemCode);
    query.bindValue(":item_name", itemName);
    query.bindValue(":category", category.isEmpty() ? QStringLiteral("检查") : category);
    query.bindValue(":unit_price", unitPrice);
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "检查项目已保存。", {}};
}

common::Response disableExaminationItemInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString itemCode = payload.value("项目编码").toString(payload.value("itemCode").toString()).trimmed();
    const QString itemName = payload.value("检查项目").toString(payload.value("itemName").toString()).trimmed();
    if (itemCode.isEmpty() && itemName.isEmpty()) {
        return {false, "请选择要停用的检查项目。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE examination_items SET status = 0 "
                  "WHERE item_code = :item_code OR item_name = :item_name");
    query.bindValue(":item_code", itemCode);
    query.bindValue(":item_name", itemName);
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        return {false, "未找到要停用的检查项目。", {}};
    }
    return {true, "检查项目已停用。", {}};
}

common::Response createExaminationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString itemName = payload.value("检查项目").toString().trimmed();
    const QString requestNote = payload.value("申请说明").toString().trimmed();
    if (registrationNo.isEmpty() || itemName.isEmpty()) {
        return {false, "挂号单号和检查项目不能为空。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, doctor_id, status FROM registrations "
                  "WHERE registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", registrationNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到该挂号单，请先完成挂号。", {}};
    }

    const qint64 registrationId = query.value(0).toLongLong();
    const qint64 doctorId = query.value(1).toLongLong();
    const QString registrationStatus = query.value(2).toString();
    query.finish();
    if (registrationStatus == "FINISHED") {
        db.rollback();
        return {false, "该挂号单已完成接诊，不能再开立检查单。", {}};
    }

    query.prepare("SELECT id, unit_price, item_name FROM examination_items "
                  "WHERE status = 1 AND (item_name = :item_name OR item_code = :item_code) LIMIT 1");
    query.bindValue(":item_name", itemName);
    query.bindValue(":item_code", itemName);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "检查项目字典中未找到该项目，请先在检查项目维护中新增并启用。", {}};
    }
    const qint64 itemId = query.value(0).toLongLong();
    const double unitPrice = query.value(1).toDouble();
    const QString canonicalItemName = query.value(2).toString();
    query.finish();

    query.prepare("SELECT examination_no FROM examinations "
                  "WHERE registration_id = :registration_id "
                  "AND status = 'PENDING' "
                  "AND (item_id = :item_id OR item_name = :item_name) LIMIT 1");
    query.bindValue(":registration_id", registrationId);
    query.bindValue(":item_id", itemId);
    query.bindValue(":item_name", canonicalItemName);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.next()) {
        db.rollback();
        return {false, "该挂号单已有待检查的相同检查项目，请勿重复开立。", {}};
    }
    query.finish();

    const QString examNo = "EX" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    query.prepare("INSERT INTO examinations "
                  "(examination_no, registration_id, doctor_id, item_id, item_name, unit_price, request_note, result_text, status) "
                  "VALUES (:examination_no, :registration_id, :doctor_id, :item_id, :item_name, :unit_price, :request_note, '', 'PENDING')");
    query.bindValue(":examination_no", examNo);
    query.bindValue(":registration_id", registrationId);
    query.bindValue(":doctor_id", doctorId);
    query.bindValue(":item_id", itemId);
    query.bindValue(":item_name", canonicalItemName);
    query.bindValue(":unit_price", unitPrice);
    query.bindValue(":request_note", requestNote);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE bills SET other_fee = other_fee + :exam_fee, "
                  "total_amount = total_amount + :exam_fee "
                  "WHERE registration_id = :registration_id AND status <> 'REFUNDED'");
    query.bindValue(":exam_fee", unitPrice);
    query.bindValue(":registration_id", registrationId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE registrations SET status = :checking WHERE id = :id AND status <> :finished");
    query.bindValue(":checking", WorkflowRules::statusAfterExaminationRequested());
    query.bindValue(":finished", WorkflowRules::finished());
    query.bindValue(":id", registrationId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data;
    data["检查单号"] = examNo;
    data["检查费用"] = unitPrice;
    return {true, "检查申请已开立，患者已转入检查中。", data};
}

common::Response completeExaminationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString examNo = payload.value("检查单号").toString().trimmed();
    const QString finding = payload.value("报告所见").toString(payload.value("检查结果").toString()).trimmed();
    const QString conclusion = payload.value("报告结论").toString().trimmed();
    const QString attachment = payload.value("报告附件").toString().trimmed();
    const QString resultText = conclusion.isEmpty() ? finding : conclusion;
    if (examNo.isEmpty() || finding.isEmpty() || conclusion.isEmpty()) {
        return {false, "检查单号、报告所见和报告结论不能为空。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("UPDATE examinations SET result_text = :result_text, report_finding = :report_finding, "
                  "report_conclusion = :report_conclusion, report_attachment = :report_attachment, status = 'COMPLETED', "
                  "complete_time = CURRENT_TIMESTAMP WHERE examination_no = :examination_no");
    query.bindValue(":result_text", resultText);
    query.bindValue(":report_finding", finding);
    query.bindValue(":report_conclusion", conclusion);
    query.bindValue(":report_attachment", attachment);
    query.bindValue(":examination_no", examNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        return {false, "未找到该检查单。", {}};
    }

    query.prepare("UPDATE registrations r "
                  "JOIN examinations e ON e.registration_id = r.id "
                  "SET r.status = :check_done "
                  "WHERE e.examination_no = :examination_no AND r.status = :checking");
    query.bindValue(":check_done", WorkflowRules::statusAfterExaminationCompleted());
    query.bindValue(":checking", WorkflowRules::checking());
    query.bindValue(":examination_no", examNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, "检查结果已回传，患者已进入检查完成待复诊。", {}};
}

} // namespace

ExaminationService::ExaminationService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response ExaminationService::handle(const common::Request& request)
{
    if (request.action == "items") {
        if (m_database->isEnabled()) {
            return listExaminationItemsInDatabase(m_database);
        }
        QJsonObject data;
        data["rows"] = QJsonArray{
            QJsonObject{{"项目编码", "EXAM_CT"}, {"检查项目", "CT"}, {"项目分类", "影像检查"}, {"单价", 260.0}, {"状态", "启用"}},
            QJsonObject{{"项目编码", "EXAM_BLOOD"}, {"检查项目", "血常规"}, {"项目分类", "检验"}, {"单价", 25.0}, {"状态", "启用"}}
        };
        data["count"] = data.value("rows").toArray().size();
        return {true, "OK", data};
    }

    if (request.action == "saveItem") {
        if (m_database->isEnabled()) {
            return saveExaminationItemInDatabase(m_database, request.payload);
        }
        return {true, "Demo 检查项目已保存。", {}};
    }

    if (request.action == "deleteItem") {
        if (m_database->isEnabled()) {
            return disableExaminationItemInDatabase(m_database, request.payload);
        }
        return {true, "Demo 检查项目已停用。", {}};
    }

    if (request.action == "create") {
        if (m_database->isEnabled()) {
            return createExaminationInDatabase(m_database, request.payload);
        }
        const auto result = DemoRepository::instance().createExamination(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }

    if (request.action == "complete") {
        if (m_database->isEnabled()) {
            return completeExaminationInDatabase(m_database, request.payload);
        }
        const auto result = DemoRepository::instance().completeExamination(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }

    if (request.action != "list") {
        return {false, "Unsupported examination action", {}};
    }

    if (!m_database->isEnabled()) {
        const auto rows = DemoRepository::instance().rows("examinations", request.payload.value("keyword").toString());
        QJsonObject data;
        data["rows"] = rows;
        data["count"] = rows.size();
        return {true, "OK", data};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    QStringList filters;
    QVariantMap params;
    if (!keyword.isEmpty()) {
        filters.append("(e.examination_no LIKE CONCAT('%', :keyword_exam, '%') "
                       "OR r.registration_no LIKE CONCAT('%', :keyword_registration, '%') "
                       "OR p.name LIKE CONCAT('%', :keyword_patient, '%') "
                       "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%') "
                       "OR u.real_name LIKE CONCAT('%', :keyword_doctor, '%') "
                       "OR e.item_name LIKE CONCAT('%', :keyword_item, '%'))");
        params.insert("keyword_exam", keyword);
        params.insert("keyword_registration", keyword);
        params.insert("keyword_patient", keyword);
        params.insert("keyword_id_card", keyword);
        params.insert("keyword_doctor", keyword);
        params.insert("keyword_item", keyword);
    }
    const QString whereClause = filters.isEmpty() ? QString() : "WHERE " + filters.join(" AND ") + " ";

    return SqlJson::selectRows(m_database,
        "SELECT e.examination_no AS '检查单号', r.registration_no AS '挂号单号', "
        "p.name AS '患者', p.id_card AS '身份证号', u.real_name AS '医生', e.item_name AS '检查项目', "
        "e.unit_price AS '单价', COALESCE(e.request_note, '') AS '申请说明', "
        "COALESCE(e.report_finding, e.result_text, '') AS '报告所见', "
        "COALESCE(e.report_conclusion, e.result_text, '') AS '报告结论', "
        "COALESCE(e.report_attachment, '') AS '报告附件', "
        "COALESCE(e.result_text, '') AS '检查结果', "
        "CASE e.status WHEN 'PENDING' THEN '待检查' WHEN 'COMPLETED' THEN '已完成' ELSE e.status END AS '状态', "
        "e.request_time AS '申请时间', COALESCE(e.complete_time, '') AS '完成时间' "
        "FROM examinations e "
        "JOIN registrations r ON r.id = e.registration_id "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = e.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        + whereClause +
        "ORDER BY e.request_time DESC LIMIT 200",
        params, "examinations");
}

} // namespace hospital::server
