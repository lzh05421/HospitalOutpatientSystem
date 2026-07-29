#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QJsonArray>
#include <QVariant>

namespace hospital::server {
namespace {

common::Response updateRecordInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT r.id, r.doctor_id, COALESCE(m.chief_complaint, ''), "
                  "COALESCE(m.present_illness, ''), COALESCE(m.past_history, ''), "
                  "COALESCE(m.physical_sign, ''), COALESCE(m.icd_code, ''), "
                  "COALESCE(m.external_report_hospital, ''), COALESCE(m.external_report_type, ''), "
                  "COALESCE(m.external_report_date, ''), COALESCE(m.external_report_summary, ''), "
                  "COALESCE(m.external_report_conclusion, ''), COALESCE(m.external_report_attachment, ''), "
                  "COALESCE(m.diagnosis, ''), COALESCE(m.advice, '') "
                  "FROM registrations r LEFT JOIN medical_records m ON m.registration_id = r.id "
                  "WHERE r.registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    if (!query.exec() || !query.next()) {
        return {false, "未找到对应就诊记录。", {}};
    }

    const qint64 registrationId = query.value(0).toLongLong();
    const qint64 doctorId = query.value(1).toLongLong();
    const QString oldComplaint = query.value(2).toString();
    const QString oldPresentIllness = query.value(3).toString();
    const QString oldPastHistory = query.value(4).toString();
    const QString oldPhysicalSign = query.value(5).toString();
    const QString oldIcdCode = query.value(6).toString();
    const QString oldExternalReportHospital = query.value(7).toString();
    const QString oldExternalReportType = query.value(8).toString();
    const QString oldExternalReportDate = query.value(9).toString();
    const QString oldExternalReportSummary = query.value(10).toString();
    const QString oldExternalReportConclusion = query.value(11).toString();
    const QString oldExternalReportAttachment = query.value(12).toString();
    const QString oldDiagnosis = query.value(13).toString();
    const QString oldAdvice = query.value(14).toString();
    query.finish();
    const QString newComplaint = payload.value("主诉").toString();
    const QString newPresentIllness = payload.value("现病史").toString();
    const QString newPastHistory = payload.value("既往史").toString();
    const QString newPhysicalSign = payload.value("体格检查").toString();
    const QString newIcdCode = payload.value("ICD编码").toString();
    const QString newExternalReportHospital = payload.value("外院报告医院").toString();
    const QString newExternalReportType = payload.value("外院报告类型").toString();
    const QString newExternalReportDate = payload.value("外院报告日期").toString();
    const QString newExternalReportSummary = payload.value("外院报告摘要").toString();
    const QString newExternalReportConclusion = payload.value("外院报告结论").toString();
    const QString newExternalReportAttachment = payload.value("外院报告附件").toString();
    const QString newDiagnosis = payload.value("诊断").toString();
    const QString newAdvice = payload.value("医嘱").toString();
    query.prepare("INSERT INTO medical_records "
                  "(registration_id, chief_complaint, present_illness, past_history, physical_sign, icd_code, diagnosis, advice, "
                  "external_report_hospital, external_report_type, external_report_date, external_report_summary, "
                  "external_report_conclusion, external_report_attachment, doctor_id) "
                  "VALUES (:registration_id, :chief_complaint, :present_illness, :past_history, :physical_sign, :icd_code, :diagnosis, :advice, "
                  ":external_report_hospital, :external_report_type, :external_report_date, :external_report_summary, "
                  ":external_report_conclusion, :external_report_attachment, :doctor_id) "
                  "ON DUPLICATE KEY UPDATE chief_complaint = VALUES(chief_complaint), "
                  "present_illness = VALUES(present_illness), past_history = VALUES(past_history), "
                  "physical_sign = VALUES(physical_sign), icd_code = VALUES(icd_code), "
                  "diagnosis = VALUES(diagnosis), advice = VALUES(advice), "
                  "external_report_hospital = VALUES(external_report_hospital), external_report_type = VALUES(external_report_type), "
                  "external_report_date = VALUES(external_report_date), external_report_summary = VALUES(external_report_summary), "
                  "external_report_conclusion = VALUES(external_report_conclusion), external_report_attachment = VALUES(external_report_attachment), "
                  "created_at = CURRENT_TIMESTAMP");
    query.bindValue(":registration_id", registrationId);
    query.bindValue(":chief_complaint", newComplaint);
    query.bindValue(":present_illness", newPresentIllness);
    query.bindValue(":past_history", newPastHistory);
    query.bindValue(":physical_sign", newPhysicalSign);
    query.bindValue(":icd_code", newIcdCode);
    query.bindValue(":diagnosis", newDiagnosis);
    query.bindValue(":advice", newAdvice);
    query.bindValue(":external_report_hospital", newExternalReportHospital);
    query.bindValue(":external_report_type", newExternalReportType);
    query.bindValue(":external_report_date", newExternalReportDate.isEmpty() ? QVariant() : QVariant(newExternalReportDate));
    query.bindValue(":external_report_summary", newExternalReportSummary);
    query.bindValue(":external_report_conclusion", newExternalReportConclusion);
    query.bindValue(":external_report_attachment", newExternalReportAttachment);
    query.bindValue(":doctor_id", doctorId);
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    QJsonArray details;
    const QString key = payload.value("挂号单号").toString();
    auto appendDetail = [&](const QString& field, const QString& oldValue, const QString& newValue) {
        if (oldValue == newValue) {
            return;
        }
        details.append(QJsonObject{{"businessKey", key}, {"fieldName", field},
                                   {"oldValue", oldValue}, {"newValue", newValue},
                                   {"changeReason", payload.value("修改原因").toString("病历维护")}});
    };
    appendDetail("主诉", oldComplaint, newComplaint);
    appendDetail("现病史", oldPresentIllness, newPresentIllness);
    appendDetail("既往史", oldPastHistory, newPastHistory);
    appendDetail("体格检查", oldPhysicalSign, newPhysicalSign);
    appendDetail("ICD编码", oldIcdCode, newIcdCode);
    appendDetail("外院报告医院", oldExternalReportHospital, newExternalReportHospital);
    appendDetail("外院报告类型", oldExternalReportType, newExternalReportType);
    appendDetail("外院报告日期", oldExternalReportDate, newExternalReportDate);
    appendDetail("外院报告摘要", oldExternalReportSummary, newExternalReportSummary);
    appendDetail("外院报告结论", oldExternalReportConclusion, newExternalReportConclusion);
    appendDetail("外院报告附件", oldExternalReportAttachment, newExternalReportAttachment);
    appendDetail("诊断", oldDiagnosis, newDiagnosis);
    appendDetail("医嘱", oldAdvice, newAdvice);
    QJsonObject data;
    data["__auditDetails"] = details;
    return {true, "病历内容已修改。", data};
}

common::Response deleteRecordInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT COALESCE(m.chief_complaint, ''), COALESCE(m.present_illness, ''), "
                  "COALESCE(m.past_history, ''), COALESCE(m.physical_sign, ''), "
                  "COALESCE(m.icd_code, ''), COALESCE(m.external_report_hospital, ''), "
                  "COALESCE(m.external_report_type, ''), COALESCE(m.external_report_date, ''), "
                  "COALESCE(m.external_report_summary, ''), COALESCE(m.external_report_conclusion, ''), "
                  "COALESCE(m.external_report_attachment, ''), COALESCE(m.diagnosis, ''), COALESCE(m.advice, '') "
                  "FROM medical_records m JOIN registrations r ON r.id = m.registration_id "
                  "WHERE r.registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    QString oldComplaint;
    QString oldPresentIllness;
    QString oldPastHistory;
    QString oldPhysicalSign;
    QString oldIcdCode;
    QString oldExternalReportHospital;
    QString oldExternalReportType;
    QString oldExternalReportDate;
    QString oldExternalReportSummary;
    QString oldExternalReportConclusion;
    QString oldExternalReportAttachment;
    QString oldDiagnosis;
    QString oldAdvice;
    if (query.exec() && query.next()) {
        oldComplaint = query.value(0).toString();
        oldPresentIllness = query.value(1).toString();
        oldPastHistory = query.value(2).toString();
        oldPhysicalSign = query.value(3).toString();
        oldIcdCode = query.value(4).toString();
        oldExternalReportHospital = query.value(5).toString();
        oldExternalReportType = query.value(6).toString();
        oldExternalReportDate = query.value(7).toString();
        oldExternalReportSummary = query.value(8).toString();
        oldExternalReportConclusion = query.value(9).toString();
        oldExternalReportAttachment = query.value(10).toString();
        oldDiagnosis = query.value(11).toString();
        oldAdvice = query.value(12).toString();
    }
    query.finish();
    query.prepare("UPDATE medical_records m "
                  "JOIN registrations r ON r.id = m.registration_id "
                  "SET m.chief_complaint = '病历已作废', m.present_illness = '病历已作废', "
                  "m.past_history = '病历已作废', m.physical_sign = '病历已作废', "
                  "m.icd_code = '', m.diagnosis = '病历已作废', "
                  "m.advice = '病历已作废', m.external_report_hospital = '', "
                  "m.external_report_type = '', m.external_report_date = NULL, "
                  "m.external_report_summary = '', m.external_report_conclusion = '', "
                  "m.external_report_attachment = '', m.created_at = CURRENT_TIMESTAMP "
                  "WHERE r.registration_no = :registration_no");
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        return {false, "该就诊记录还没有病历内容，无需作废。", {}};
    }
    QJsonObject data;
    data["__auditDetails"] = QJsonArray{
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "主诉"}, {"oldValue", oldComplaint}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "现病史"}, {"oldValue", oldPresentIllness}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "既往史"}, {"oldValue", oldPastHistory}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "体格检查"}, {"oldValue", oldPhysicalSign}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "ICD编码"}, {"oldValue", oldIcdCode}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告医院"}, {"oldValue", oldExternalReportHospital}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告类型"}, {"oldValue", oldExternalReportType}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告日期"}, {"oldValue", oldExternalReportDate}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告摘要"}, {"oldValue", oldExternalReportSummary}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告结论"}, {"oldValue", oldExternalReportConclusion}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "外院报告附件"}, {"oldValue", oldExternalReportAttachment}, {"newValue", ""}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "诊断"}, {"oldValue", oldDiagnosis}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}},
        QJsonObject{{"businessKey", payload.value("挂号单号").toString()}, {"fieldName", "医嘱"}, {"oldValue", oldAdvice}, {"newValue", "病历已作废"}, {"changeReason", "病历作废"}}
    };
    return {true, "病历内容已作废，就诊流水已保留。", data};
}

} // namespace

PatientRecordService::PatientRecordService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response PatientRecordService::handle(const common::Request& request)
{
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updatePatientRecord(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateRecordInDatabase(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().deletePatientRecord(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return deleteRecordInDatabase(m_database, request.payload);
    }
    if (request.action != "list") {
        return {false, "Unsupported patient record action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    if (!m_database->isEnabled()) {
        const auto rows = DemoRepository::instance().patientRecords(keyword);
        QJsonObject data;
        data["rows"] = rows;
        data["count"] = rows.size();
        return {true, "Demo data", data};
    }

    const QString operatorRoleCode = request.payload.value("__operatorRoleCode").toString();
    const QString operatorName = request.payload.value("__operatorName").toString().trimmed();
    QStringList filters;
    QVariantMap params{{"keyword", keyword}};
    if (operatorRoleCode == "DOCTOR" && !operatorName.isEmpty()) {
        filters.append("u.real_name = :operator_name");
        params.insert("operator_name", operatorName);
    } else if (operatorRoleCode == "DIRECTOR" && !operatorName.isEmpty()) {
        filters.append("(:operator_dept LIKE CONCAT('%', d.dept_name, '%') "
                       "OR (:operator_internal LIKE '%内科%' AND d.dept_code LIKE 'DEP001%') "
                       "OR (:operator_surgery LIKE '%外科%' AND d.dept_code LIKE 'DEP002%') "
                       "OR (:operator_pediatrics LIKE '%儿科%' AND d.dept_code LIKE 'DEP003%') "
                       "OR (:operator_tcm LIKE '%中医%' AND d.dept_code LIKE 'DEP004%'))");
        params.insert("operator_dept", operatorName);
        params.insert("operator_internal", operatorName);
        params.insert("operator_surgery", operatorName);
        params.insert("operator_pediatrics", operatorName);
        params.insert("operator_tcm", operatorName);
    }
    if (!keyword.isEmpty()) {
        filters.append("(p.patient_no LIKE CONCAT('%', :keyword_patient_no, '%') "
                  "OR p.name LIKE CONCAT('%', :keyword_patient_name, '%') "
                  "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%') "
                  "OR r.registration_no LIKE CONCAT('%', :keyword_registration, '%') "
                  "OR u.real_name LIKE CONCAT('%', :keyword_doctor, '%') "
                  "OR d.dept_name LIKE CONCAT('%', :keyword_department, '%'))");
        params.insert("keyword_patient_no", keyword);
        params.insert("keyword_patient_name", keyword);
        params.insert("keyword_id_card", keyword);
        params.insert("keyword_registration", keyword);
        params.insert("keyword_doctor", keyword);
        params.insert("keyword_department", keyword);
    }
    const QString whereSql = filters.isEmpty() ? QString() : "WHERE " + filters.join(" AND ") + " ";

    return SqlJson::selectRows(m_database,
        "SELECT p.patient_no AS '患者编号', p.name AS '患者', p.phone AS '电话', "
        "p.id_card AS '身份证号', "
        "r.registration_no AS '挂号单号', d.dept_name AS '科室', u.real_name AS '医生', "
        "s.work_date AS '就诊日期', COALESCE(NULLIF(r.appointment_time_slot, ''), s.period) AS '时段', "
        "CASE r.status WHEN 'WAITING' THEN '待叫号' WHEN 'CHECKING' THEN '检查中' "
        "WHEN 'CALLED' THEN '已叫号' WHEN 'IN_CONSULTATION' THEN '接诊中' "
        "WHEN 'CHECK_DONE' THEN '检查完成待复诊' WHEN 'FINISHED' THEN '已接诊' "
        "WHEN 'CANCELLED' THEN '已取消' ELSE r.status END AS '挂号状态', "
        "COALESCE(m.chief_complaint, '') AS '主诉', COALESCE(m.present_illness, '') AS '现病史', "
        "COALESCE(m.past_history, '') AS '既往史', COALESCE(m.physical_sign, '') AS '体格检查', "
        "COALESCE(m.icd_code, '') AS 'ICD编码', COALESCE(m.diagnosis, '') AS '诊断', "
        "COALESCE(m.advice, '') AS '医嘱', "
        "COALESCE(m.external_report_hospital, '') AS '外院报告医院', "
        "COALESCE(m.external_report_type, '') AS '外院报告类型', "
        "COALESCE(m.external_report_date, '') AS '外院报告日期', "
        "COALESCE(m.external_report_summary, '') AS '外院报告摘要', "
        "COALESCE(m.external_report_conclusion, '') AS '外院报告结论', "
        "COALESCE(m.external_report_attachment, '') AS '外院报告附件', "
        "COALESCE(m.created_at, '') AS '接诊时间', "
        "COALESCE(pr.prescription_no, '') AS '处方号', "
        "CASE pr.status WHEN 'CREATED' THEN '待审核' WHEN 'REVIEWED' THEN '待发药' "
        "WHEN 'DISPENSED' THEN '已发药' WHEN 'PAID' THEN '已缴费' "
        "WHEN 'CANCELLED' THEN '已取消' ELSE COALESCE(pr.status, '') END AS '处方状态', "
        "COALESCE(pr.total_amount, 0) AS '处方金额', COALESCE(b.bill_no, '') AS '账单号', "
        "COALESCE(b.total_amount, 0) AS '费用合计', "
        "CASE b.status WHEN 'UNPAID' THEN '待缴费' WHEN 'PAID' THEN '已缴费' "
        "WHEN 'REFUNDED' THEN '已退费' WHEN 'CANCELLED' THEN '已取消' ELSE COALESCE(b.status, '') END AS '账单状态' "
        "FROM registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        "LEFT JOIN medical_records m ON m.registration_id = r.id "
        "LEFT JOIN prescriptions pr ON pr.registration_id = r.id "
        "LEFT JOIN bills b ON b.registration_id = r.id "
        + whereSql +
        "ORDER BY r.register_time DESC LIMIT 200",
        params, "patientRecords");
}

} // namespace hospital::server
