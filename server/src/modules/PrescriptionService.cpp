#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QDate>
#include <QJsonArray>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

namespace hospital::server {
namespace {

qint64 operatorUserIdFromPayload(const QJsonObject& payload)
{
    const QString operatorId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    return operatorId.isEmpty() ? 1 : operatorId.toLongLong();
}

bool appendOperationLog(QSqlDatabase& db,
                        qint64 userId,
                        const QString& action,
                        const QString& content,
                        QString* error)
{
    QSqlQuery log(db);
    log.prepare("INSERT INTO operation_logs (user_id, module, action, content) "
                "VALUES (:user_id, 'prescription', :action, :content)");
    log.bindValue(":user_id", userId);
    log.bindValue(":action", action.left(64));
    log.bindValue(":content", content.left(500));
    if (!log.exec()) {
        if (error) {
            *error = log.lastError().text();
        }
        return false;
    }
    return true;
}

common::Response createPrescriptionInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString drugName = payload.value("药品名称").toString().trimmed();
    const int quantity = payload.value("数量").toVariant().toInt();
    const QString dosage = payload.value("用法用量").toString().trimmed();
    const QString frequency = payload.value("频次").toString().trimmed();
    const int days = payload.value("天数").toVariant().toInt();
    if (registrationNo.isEmpty() || drugName.isEmpty() || quantity <= 0) {
        return {false, "挂号单号、药品名称和数量不能为空。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT r.id, r.doctor_id, r.status, p.name, p.birthday "
                  "FROM registrations r JOIN patients p ON p.id = r.patient_id "
                  "WHERE r.registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", registrationNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到该挂号单，请先完成挂号。", {}};
    }

    const qint64 registrationId = query.value(0).toLongLong();
    const qint64 doctorId = query.value(1).toLongLong();
    const QString registrationStatus = query.value(2).toString();
    const QString patientBirthday = query.value(4).toDate().toString("yyyy-MM-dd");
    query.finish();
    if (registrationStatus != "FINISHED") {
        db.rollback();
        return {false, "该患者还未完成接诊，请先在医生接诊中保存病历后再开处方。", {}};
    }

    query.prepare("SELECT id, sale_price, stock_quantity, drug_name FROM drugs "
                  "WHERE status = 1 AND (drug_name = :drug_name_lookup OR drug_code = :drug_code_lookup OR barcode = :barcode_lookup) LIMIT 1");
    query.bindValue(":drug_name_lookup", drugName);
    query.bindValue(":drug_code_lookup", drugName);
    query.bindValue(":barcode_lookup", drugName);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "药品库存中未找到该药品，请先在药品库存中入库。", {}};
    }

    const qint64 drugId = query.value(0).toLongLong();
    const double unitPrice = query.value(1).toDouble();
    const QString canonicalDrugName = query.value(3).toString();
    query.finish();

    QJsonArray passWarnings;
    const QString patientCondition = payload.value("过敏史").toString() + " " + payload.value("患者情况").toString();
    query.prepare("SELECT rule_type, related_drug_name, patient_condition, warning_level, message "
                  "FROM pass_rules WHERE enabled = 1 AND drug_name = :drug_name");
    query.bindValue(":drug_name", canonicalDrugName);
    if (query.exec()) {
        while (query.next()) {
            const QString ruleType = query.value(0).toString();
            const QString relatedDrug = query.value(1).toString();
            const QString condition = query.value(2).toString();
            const QString level = query.value(3).toString();
            const QString message = query.value(4).toString();
            bool hit = false;
            if (ruleType == "ALLERGY") {
                hit = !condition.isEmpty() && patientCondition.contains(condition);
            } else if (ruleType == "DOSE") {
                const QDate birthday = QDate::fromString(patientBirthday, "yyyy-MM-dd");
                hit = (birthday.isValid() && birthday.daysTo(QDate::currentDate()) < 14 * 365)
                    || quantity > 3 || days > 3;
            } else if (ruleType == "COMBO" && !relatedDrug.isEmpty()) {
                QSqlQuery comboQuery(db);
                comboQuery.prepare("SELECT COUNT(*) FROM prescriptions pr "
                                   "JOIN prescription_items pi ON pi.prescription_id = pr.id "
                                   "JOIN drugs d ON d.id = pi.drug_id "
                                   "WHERE pr.registration_id = :registration_id AND d.drug_name = :related_drug");
                comboQuery.bindValue(":registration_id", registrationId);
                comboQuery.bindValue(":related_drug", relatedDrug);
                hit = comboQuery.exec() && comboQuery.next() && comboQuery.value(0).toInt() > 0;
            }
            if (hit) {
                passWarnings.append(QJsonObject{{"level", level}, {"message", message}});
                if (level == "BLOCK") {
                    db.rollback();
                    QJsonObject data;
                    data["passWarnings"] = passWarnings;
                    return {false, "PASS 合理用药拦截：" + message, data};
                }
            }
        }
    }
    query.finish();

    const double amount = unitPrice * quantity;
    const QString prescriptionNo = "RX" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");

    query.prepare("INSERT INTO prescriptions (prescription_no, registration_id, doctor_id, status, total_amount) "
                  "VALUES (:prescription_no, :registration_id, :doctor_id, 'CREATED', :total_amount)");
    query.bindValue(":prescription_no", prescriptionNo);
    query.bindValue(":registration_id", registrationId);
    query.bindValue(":doctor_id", doctorId);
    query.bindValue(":total_amount", amount);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    const qint64 prescriptionId = query.lastInsertId().toLongLong();

    query.prepare("INSERT INTO prescription_items "
                  "(prescription_id, drug_id, quantity, dosage, frequency, days, unit_price, amount) "
                  "VALUES (:prescription_id, :drug_id, :quantity, :dosage, :frequency, :days, :unit_price, :amount)");
    query.bindValue(":prescription_id", prescriptionId);
    query.bindValue(":drug_id", drugId);
    query.bindValue(":quantity", quantity);
    query.bindValue(":dosage", dosage);
    query.bindValue(":frequency", frequency);
    query.bindValue(":days", days);
    query.bindValue(":unit_price", unitPrice);
    query.bindValue(":amount", amount);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE bills SET drug_fee = drug_fee + :drug_amount, total_amount = total_amount + :total_amount_delta "
                  "WHERE registration_id = :registration_id AND status <> 'PAID'");
    query.bindValue(":drug_amount", amount);
    query.bindValue(":total_amount_delta", amount);
    query.bindValue(":registration_id", registrationId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data;
    data["处方号"] = prescriptionNo;
    data["处方金额"] = amount;
    data["passWarnings"] = passWarnings;
    const QString passMessage = passWarnings.isEmpty()
        ? QString()
        : QString(" PASS 提示：%1").arg(passWarnings.first().toObject().value("message").toString());
    return {true, "处方已开立，等待药师审核。" + passMessage, data};
}

common::Response reviewPrescriptionInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }
    const QString prescriptionNo = payload.value("处方号").toString().trimmed();
    const qint64 operatorUserId = operatorUserIdFromPayload(payload);
    QSqlQuery query(db);
    query.prepare("UPDATE prescriptions SET status = 'REVIEWED', reviewer_id = :reviewer_id, review_time = NOW(), reject_reason = NULL "
                  "WHERE prescription_no = :prescription_no AND status = 'CREATED'");
    query.bindValue(":reviewer_id", operatorUserId);
    query.bindValue(":prescription_no", prescriptionNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        return {false, "处方不存在，或当前状态不能审核。", {}};
    }
    QString error;
    if (!appendOperationLog(db, operatorUserId, "审核处方", "审核通过 " + prescriptionNo, &error)) {
        db.rollback();
        return {false, error, {}};
    }
    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "处方已审核，等待药房发药。", {}};
}

common::Response rejectPrescriptionInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString prescriptionNo = payload.value("处方号").toString().trimmed();
    const QString rejectReason = payload.value("驳回原因").toString().trimmed();
    if (prescriptionNo.isEmpty()) {
        return {false, "请选择要驳回的处方。", {}};
    }
    if (rejectReason.isEmpty()) {
        return {false, "驳回处方必须填写驳回原因。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }
    const qint64 operatorUserId = operatorUserIdFromPayload(payload);
    QSqlQuery query(db);
    query.prepare("UPDATE prescriptions SET status = 'REJECTED', reviewer_id = :reviewer_id, "
                  "review_time = NOW(), reject_reason = :reject_reason "
                  "WHERE prescription_no = :prescription_no AND status = 'CREATED'");
    query.bindValue(":reviewer_id", operatorUserId);
    query.bindValue(":reject_reason", rejectReason);
    query.bindValue(":prescription_no", prescriptionNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        return {false, "处方不存在，或当前状态不能驳回。", {}};
    }

    QString error;
    if (!appendOperationLog(db, operatorUserId, "驳回处方", QString("驳回 %1：%2").arg(prescriptionNo, rejectReason), &error)) {
        db.rollback();
        return {false, error, {}};
    }
    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "处方已驳回，医生需重新开立处方。", {}};
}

common::Response dispensePrescriptionInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const QString prescriptionNo = payload.value("处方号").toString().trimmed();
    QSqlQuery query(db);
    query.prepare("SELECT id, status FROM prescriptions WHERE prescription_no = :prescription_no LIMIT 1");
    query.bindValue(":prescription_no", prescriptionNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到该处方。", {}};
    }

    const qint64 prescriptionId = query.value(0).toLongLong();
    const QString status = query.value(1).toString();
    query.finish();
    if (status == "DISPENSED") {
        db.rollback();
        return {false, "该处方已经发药，不能重复发药。", {}};
    }
    if (status == "REJECTED") {
        db.rollback();
        return {false, "该处方已驳回，不能发药。请医生重新开立处方。", {}};
    }
    if (status == "RETURNED") {
        db.rollback();
        return {false, "该处方已退药，不能再次发药。", {}};
    }
    if (status != "REVIEWED") {
        db.rollback();
        return {false, "请先审核处方，再确认发药。", {}};
    }

    query.prepare("SELECT COALESCE(b.status, '') "
                  "FROM prescriptions pr "
                  "JOIN registrations r ON r.id = pr.registration_id "
                  "JOIN bills b ON b.registration_id = r.id "
                  "WHERE pr.id = :prescription_id LIMIT 1");
    query.bindValue(":prescription_id", prescriptionId);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到该处方对应账单，不能发药。", {}};
    }
    const QString billStatus = query.value(0).toString();
    query.finish();
    if (billStatus != "PAID") {
        db.rollback();
        return {false, "该处方对应账单未缴费，不能发药。请先完成收费结算。", {}};
    }

    query.prepare("SELECT pi.drug_id, pi.quantity, d.stock_quantity "
                  "FROM prescription_items pi "
                  "JOIN drugs d ON d.id = pi.drug_id "
                  "WHERE pi.prescription_id = :prescription_id FOR UPDATE");
    query.bindValue(":prescription_id", prescriptionId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    struct Item {
        qint64 drugId = 0;
        int quantity = 0;
        int beforeQuantity = 0;
    };
    QVector<Item> items;
    while (query.next()) {
        Item item;
        item.drugId = query.value(0).toLongLong();
        item.quantity = query.value(1).toInt();
        item.beforeQuantity = query.value(2).toInt();
        if (item.beforeQuantity < item.quantity) {
            db.rollback();
            return {false, QString("库存不足，当前库存 %1。").arg(item.beforeQuantity), {}};
        }
        items.append(item);
    }
    query.finish();
    if (items.isEmpty()) {
        db.rollback();
        return {false, "处方没有药品明细，不能发药。", {}};
    }

    const qint64 operatorUserId = operatorUserIdFromPayload(payload);
    for (const auto& item : items) {
        query.prepare("UPDATE drugs SET stock_quantity = stock_quantity - :quantity WHERE id = :drug_id");
        query.bindValue(":quantity", item.quantity);
        query.bindValue(":drug_id", item.drugId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }

        query.prepare("INSERT INTO stock_records "
                      "(drug_id, change_type, quantity, before_quantity, after_quantity, related_no, operator_id) "
                      "VALUES (:drug_id, 'OUT', :quantity, :before_quantity, :after_quantity, :related_no, :operator_id)");
        query.bindValue(":drug_id", item.drugId);
        query.bindValue(":quantity", item.quantity);
        query.bindValue(":before_quantity", item.beforeQuantity);
        query.bindValue(":after_quantity", item.beforeQuantity - item.quantity);
        query.bindValue(":related_no", prescriptionNo);
        query.bindValue(":operator_id", operatorUserId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
    }

    query.prepare("UPDATE prescriptions SET status = 'DISPENSED', dispense_user_id = :dispense_user_id, dispense_time = NOW() WHERE id = :id");
    query.bindValue(":dispense_user_id", operatorUserId);
    query.bindValue(":id", prescriptionId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    QString error;
    if (!appendOperationLog(db, operatorUserId, "确认发药", "发药完成 " + prescriptionNo, &error)) {
        db.rollback();
        return {false, error, {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, "发药完成，库存已同步扣减。", {}};
}

common::Response returnPrescriptionInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString prescriptionNo = payload.value("处方号").toString().trimmed();
    const QString returnReason = payload.value("退药原因").toString().trimmed();
    if (prescriptionNo.isEmpty()) {
        return {false, "请选择要退药的处方。", {}};
    }
    if (returnReason.isEmpty()) {
        return {false, "退药必须填写退药原因。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, status FROM prescriptions WHERE prescription_no = :prescription_no LIMIT 1 FOR UPDATE");
    query.bindValue(":prescription_no", prescriptionNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到该处方。", {}};
    }

    const qint64 prescriptionId = query.value(0).toLongLong();
    const QString status = query.value(1).toString();
    query.finish();
    if (status == "RETURNED") {
        db.rollback();
        return {false, "该处方已退药，不能重复退药。", {}};
    }
    if (status != "DISPENSED") {
        db.rollback();
        return {false, "只有已发药处方才能退药。", {}};
    }

    query.prepare("SELECT pi.drug_id, pi.quantity, d.stock_quantity "
                  "FROM prescription_items pi "
                  "JOIN drugs d ON d.id = pi.drug_id "
                  "WHERE pi.prescription_id = :prescription_id FOR UPDATE");
    query.bindValue(":prescription_id", prescriptionId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    struct Item {
        qint64 drugId = 0;
        int quantity = 0;
        int beforeQuantity = 0;
    };
    QVector<Item> items;
    while (query.next()) {
        Item item;
        item.drugId = query.value(0).toLongLong();
        item.quantity = query.value(1).toInt();
        item.beforeQuantity = query.value(2).toInt();
        items.append(item);
    }
    query.finish();
    if (items.isEmpty()) {
        db.rollback();
        return {false, "处方没有药品明细，不能退药。", {}};
    }

    const qint64 operatorUserId = operatorUserIdFromPayload(payload);
    for (const auto& item : items) {
        query.prepare("UPDATE drugs SET stock_quantity = stock_quantity + :quantity WHERE id = :drug_id");
        query.bindValue(":quantity", item.quantity);
        query.bindValue(":drug_id", item.drugId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }

        query.prepare("INSERT INTO stock_records "
                      "(drug_id, change_type, quantity, before_quantity, after_quantity, related_no, operator_id) "
                      "VALUES (:drug_id, 'RETURN', :quantity, :before_quantity, :after_quantity, :related_no, :operator_id)");
        query.bindValue(":drug_id", item.drugId);
        query.bindValue(":quantity", item.quantity);
        query.bindValue(":before_quantity", item.beforeQuantity);
        query.bindValue(":after_quantity", item.beforeQuantity + item.quantity);
        query.bindValue(":related_no", prescriptionNo);
        query.bindValue(":operator_id", operatorUserId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
    }

    query.prepare("UPDATE prescriptions SET status = 'RETURNED', return_user_id = :return_user_id, "
                  "return_time = NOW(), return_reason = :return_reason WHERE id = :id");
    query.bindValue(":return_user_id", operatorUserId);
    query.bindValue(":return_reason", returnReason);
    query.bindValue(":id", prescriptionId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    QString error;
    if (!appendOperationLog(db, operatorUserId, "退药入库", QString("退药 %1：%2").arg(prescriptionNo, returnReason), &error)) {
        db.rollback();
        return {false, error, {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, "退药完成，库存已回加。", {}};
}

} // namespace

PrescriptionService::PrescriptionService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response PrescriptionService::handle(const common::Request& request)
{
    if (request.action == "create") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().createPrescription(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return createPrescriptionInDatabase(m_database, request.payload);
    }

    if (request.action == "review") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().reviewPrescription(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return reviewPrescriptionInDatabase(m_database, request.payload);
    }

    if (request.action == "reject") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().rejectPrescription(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return rejectPrescriptionInDatabase(m_database, request.payload);
    }

    if (request.action == "dispense") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().dispensePrescription(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return dispensePrescriptionInDatabase(m_database, request.payload);
    }

    if (request.action == "return") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().returnPrescription(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return returnPrescriptionInDatabase(m_database, request.payload);
    }

    if (request.action != "list") {
        return {false, "Unsupported prescription action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    QStringList filters;
    QVariantMap params;
    if (!keyword.isEmpty()) {
        filters.append("(pr.prescription_no LIKE CONCAT('%', :keyword_prescription, '%') "
                       "OR r.registration_no LIKE CONCAT('%', :keyword_registration, '%') "
                       "OR p.name LIKE CONCAT('%', :keyword_patient, '%') "
                       "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%') "
                       "OR u.real_name LIKE CONCAT('%', :keyword_doctor, '%') "
                       "OR dr.drug_name LIKE CONCAT('%', :keyword_drug, '%'))");
        params.insert("keyword_prescription", keyword);
        params.insert("keyword_registration", keyword);
        params.insert("keyword_patient", keyword);
        params.insert("keyword_id_card", keyword);
        params.insert("keyword_doctor", keyword);
        params.insert("keyword_drug", keyword);
    }
    const QString whereClause = filters.isEmpty() ? QString() : QString("WHERE %1 ").arg(filters.join(" AND "));

    return SqlJson::selectRows(m_database,
        "SELECT pr.prescription_no AS '处方号', r.registration_no AS '挂号单号', "
        "p.name AS '患者', p.id_card AS '身份证号', u.real_name AS '医生', "
        "CASE pr.status WHEN 'CREATED' THEN '待审核' WHEN 'REVIEWED' THEN '待发药' "
        "WHEN 'DISPENSED' THEN '已发药' WHEN 'REJECTED' THEN '已驳回' "
        "WHEN 'RETURNED' THEN '已退药' WHEN 'PAID' THEN '已缴费' ELSE pr.status END AS '状态', "
        "COALESCE(GROUP_CONCAT(CONCAT(dr.drug_name, ' x', pi.quantity) SEPARATOR '；'), '') AS '药品明细', "
        "pr.total_amount AS '处方金额', COALESCE(pr.reject_reason, '') AS '驳回原因', "
        "COALESCE(pr.return_reason, '') AS '退药原因', pr.review_time AS '审核时间', "
        "pr.dispense_time AS '发药时间', pr.return_time AS '退药时间', pr.created_at AS '开方时间' "
        "FROM prescriptions pr "
        "JOIN registrations r ON r.id = pr.registration_id "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = pr.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "LEFT JOIN prescription_items pi ON pi.prescription_id = pr.id "
        "LEFT JOIN drugs dr ON dr.id = pi.drug_id "
        + whereClause +
        "GROUP BY pr.id, pr.prescription_no, r.registration_no, p.name, p.id_card, u.real_name, pr.status, pr.total_amount, "
        "pr.reject_reason, pr.return_reason, pr.review_time, pr.dispense_time, pr.return_time, pr.created_at "
        "ORDER BY pr.created_at DESC LIMIT 100",
        params, "prescriptions");
}

} // namespace hospital::server
