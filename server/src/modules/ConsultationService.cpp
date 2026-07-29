#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"
#include "server/WorkflowRules.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantMap>

namespace hospital::server {
namespace {

bool isDoctorOperator(const QJsonObject& payload)
{
    return payload.value("__operatorRoleCode").toString().trimmed().compare("DOCTOR", Qt::CaseInsensitive) == 0;
}

QString operatorDoctorIdFromPayload(const QJsonObject& payload)
{
    return payload.value("__doctorId").toVariant().toString().trimmed();
}

common::Response missingDoctorProfileResponse()
{
    return {false, "当前医生账号未绑定医生档案，请在医生管理中关联该账号。", {}};
}

void applyVisitDateFilter(QStringList& filters, QVariantMap& params, const QJsonObject& payload)
{
    const QString dateFilter = payload.value("dateFilter").toString("今天").trimmed();
    const QDate selectedDate = QDate::fromString(payload.value("dateValue").toString().trimmed(), "yyyy-MM-dd");
    const QDate today = QDate::currentDate();

    if (dateFilter == "全部日期") {
        return;
    }
    if (dateFilter == "指定日期") {
        const QDate visitDate = selectedDate.isValid() ? selectedDate : today;
        filters.append("s.work_date = :visit_date");
        params.insert("visit_date", visitDate);
        return;
    }
    if (dateFilter == "本周") {
        const QDate weekStart = today.addDays(1 - today.dayOfWeek());
        filters.append("s.work_date BETWEEN :visit_start_date AND :visit_end_date");
        params.insert("visit_start_date", weekStart);
        params.insert("visit_end_date", weekStart.addDays(6));
        return;
    }
    if (dateFilter == "本月") {
        filters.append("s.work_date BETWEEN :visit_start_date AND :visit_end_date");
        params.insert("visit_start_date", QDate(today.year(), today.month(), 1));
        params.insert("visit_end_date", QDate(today.year(), today.month(), today.daysInMonth()));
        return;
    }

    filters.append("s.work_date = :visit_date");
    params.insert("visit_date", today);
}

common::Response startConsultationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const bool operatorIsDoctor = isDoctorOperator(payload);
    const QString operatorDoctorId = operatorDoctorIdFromPayload(payload);
    if (operatorIsDoctor && operatorDoctorId.isEmpty()) {
        return missingDoctorProfileResponse();
    }

    QSqlQuery query(database->database());
    QString sql = "UPDATE registrations SET status = :in_consultation "
                  "WHERE registration_no = :registration_no "
                  "AND (status = :called OR status = :check_done)";
    if (operatorIsDoctor) {
        sql += " AND doctor_id = :operator_doctor_id";
    }
    query.prepare(sql);
    query.bindValue(":in_consultation", WorkflowRules::inConsultation());
    query.bindValue(":registration_no", payload.value("挂号单号").toString().trimmed());
    query.bindValue(":called", WorkflowRules::called());
    query.bindValue(":check_done", WorkflowRules::checkDone());
    if (operatorIsDoctor) {
        query.bindValue(":operator_doctor_id", operatorDoctorId.toLongLong());
    }
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        if (operatorIsDoctor) {
            return {false, "当前医生账号不能操作其他医生的接诊记录。", {}};
        }
        return {false, "只有已叫号或检查完成待复诊的患者可以开始接诊。", {}};
    }
    return {true, "已进入接诊中。", {}};
}

common::Response saveConsultationInDatabase(DatabaseManager* database, const QJsonObject& payload, bool backToWaiting)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString complaint = payload.value("主诉").toString().trimmed();
    const QString presentIllness = payload.value("现病史").toString().trimmed();
    const QString pastHistory = payload.value("既往史").toString().trimmed();
    const QString physicalSign = payload.value("体格检查").toString().trimmed();
    const QString icdCode = payload.value("ICD编码").toString().trimmed();
    const QString diagnosis = payload.value("诊断").toString().trimmed();
    const QString advice = payload.value("医嘱").toString().trimmed();
    const QString externalReportHospital = payload.value("外院报告医院").toString().trimmed();
    const QString externalReportType = payload.value("外院报告类型").toString().trimmed();
    const QString externalReportDate = payload.value("外院报告日期").toString().trimmed();
    const QString externalReportSummary = payload.value("外院报告摘要").toString().trimmed();
    const QString externalReportConclusion = payload.value("外院报告结论").toString().trimmed();
    const QString externalReportAttachment = payload.value("外院报告附件").toString().trimmed();
    if (registrationNo.isEmpty() || complaint.isEmpty() || advice.isEmpty()) {
        return {false, "挂号单号、主诉和医嘱不能为空。", {}};
    }
    if (!backToWaiting && diagnosis.isEmpty()) {
        return {false, "诊断完成时必须填写诊断结果。", {}};
    }

    const bool operatorIsDoctor = isDoctorOperator(payload);
    const QString operatorDoctorId = operatorDoctorIdFromPayload(payload);
    if (operatorIsDoctor && operatorDoctorId.isEmpty()) {
        return missingDoctorProfileResponse();
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, doctor_id, status FROM registrations WHERE registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", registrationNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到对应挂号记录，请刷新候诊队列后重试。", {}};
    }

    const qint64 registrationId = query.value(0).toLongLong();
    const qint64 doctorId = query.value(1).toLongLong();
    const QString registrationStatus = query.value(2).toString();
    query.finish();
    if (operatorIsDoctor && doctorId != operatorDoctorId.toLongLong()) {
        db.rollback();
        return {false, "当前医生账号不能操作其他医生的接诊记录。", {}};
    }
    if (!WorkflowRules::canFinishConsultation(registrationStatus)) {
        db.rollback();
        return {false, "当前状态不能保存接诊，请先叫号或等待检查结果回传。", {}};
    }

    query.prepare(
        "INSERT INTO medical_records "
        "(registration_id, chief_complaint, present_illness, past_history, physical_sign, icd_code, diagnosis, advice, "
        "external_report_hospital, external_report_type, external_report_date, external_report_summary, "
        "external_report_conclusion, external_report_attachment, doctor_id) "
        "VALUES (:registration_id, :chief_complaint, :present_illness, :past_history, :physical_sign, :icd_code, :diagnosis, :advice, "
        ":external_report_hospital, :external_report_type, :external_report_date, :external_report_summary, "
        ":external_report_conclusion, :external_report_attachment, :doctor_id) "
        "ON DUPLICATE KEY UPDATE chief_complaint = VALUES(chief_complaint), present_illness = VALUES(present_illness), "
        "past_history = VALUES(past_history), physical_sign = VALUES(physical_sign), icd_code = VALUES(icd_code), "
        "diagnosis = VALUES(diagnosis), advice = VALUES(advice), "
        "external_report_hospital = VALUES(external_report_hospital), external_report_type = VALUES(external_report_type), "
        "external_report_date = VALUES(external_report_date), external_report_summary = VALUES(external_report_summary), "
        "external_report_conclusion = VALUES(external_report_conclusion), external_report_attachment = VALUES(external_report_attachment), "
        "doctor_id = VALUES(doctor_id), "
        "created_at = CURRENT_TIMESTAMP");
    query.bindValue(":registration_id", registrationId);
    query.bindValue(":chief_complaint", complaint);
    query.bindValue(":present_illness", presentIllness);
    query.bindValue(":past_history", pastHistory);
    query.bindValue(":physical_sign", physicalSign);
    query.bindValue(":icd_code", icdCode);
    query.bindValue(":diagnosis", backToWaiting && diagnosis.isEmpty() ? QStringLiteral("待检查结果回报") : diagnosis);
    query.bindValue(":advice", advice);
    query.bindValue(":external_report_hospital", externalReportHospital);
    query.bindValue(":external_report_type", externalReportType);
    query.bindValue(":external_report_date", externalReportDate.isEmpty() ? QVariant() : QVariant(externalReportDate));
    query.bindValue(":external_report_summary", externalReportSummary);
    query.bindValue(":external_report_conclusion", externalReportConclusion);
    query.bindValue(":external_report_attachment", externalReportAttachment);
    query.bindValue(":doctor_id", doctorId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE registrations SET status = :status WHERE id = :id");
    query.bindValue(":status", backToWaiting ? WorkflowRules::statusAfterExaminationRequested() : WorkflowRules::finished());
    query.bindValue(":id", registrationId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return {true, backToWaiting ? QStringLiteral("已保存当前接诊记录，患者已转入检查中，检查完成后可复诊。")
                                : QStringLiteral("接诊记录已保存，挂号状态已改为已接诊。"), {}};
}

} // namespace

ConsultationService::ConsultationService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response ConsultationService::handle(const common::Request& request)
{
    if (request.action == "start") {
        if (m_database->isEnabled()) {
            return startConsultationInDatabase(m_database, request.payload);
        }
        const auto result = DemoRepository::instance().startConsultation(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }

    if (request.action == "save" || request.action == "saveWaiting") {
        const bool backToWaiting = request.action == "saveWaiting";
        if (m_database->isEnabled()) {
            return saveConsultationInDatabase(m_database, request.payload, backToWaiting);
        }

        const auto result = DemoRepository::instance().saveConsultation(request.payload, backToWaiting);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }

    if (request.action != "list") {
        return {false, "Unsupported consultation action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    const QString departmentFilter = request.payload.value("departmentFilter").toString().trimmed();
    const QString doctorFilter = request.payload.value("doctorFilter").toString().trimmed();
    const QString clinicTypeFilter = request.payload.value("clinicTypeFilter").toString().trimmed();
    if (!m_database->isEnabled()) {
        const auto rows = DemoRepository::instance().activeConsultations(keyword, departmentFilter, doctorFilter, clinicTypeFilter);
        QJsonObject data;
        data["rows"] = rows;
        data["count"] = rows.size();
        return {true, "Demo data", data};
    }

    const QString operatorRoleCode = request.payload.value("__operatorRoleCode").toString().trimmed().toUpper();
    const QString operatorName = request.payload.value("__operatorName").toString().trimmed();
    const QString operatorDoctorId = request.payload.value("__doctorId").toVariant().toString().trimmed();
    const bool operatorIsDoctor = operatorRoleCode == "DOCTOR";
    QStringList filters = {"r.status IN ('CALLED', 'IN_CONSULTATION', 'CHECK_DONE')"};
    QVariantMap params;
    applyVisitDateFilter(filters, params, request.payload);
    if (operatorIsDoctor) {
        if (operatorDoctorId.isEmpty()) {
            return missingDoctorProfileResponse();
        }
        filters.append("r.doctor_id = :operator_doctor_id");
        params.insert("operator_doctor_id", operatorDoctorId.toLongLong());
    }
    if (operatorRoleCode == "DIRECTOR" && !operatorName.isEmpty()) {
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
    if (!departmentFilter.isEmpty()) {
        filters.append("d.dept_name = :department_filter");
        params.insert("department_filter", departmentFilter);
    }
    if (!operatorIsDoctor && !doctorFilter.isEmpty()) {
        filters.append("u.real_name = :doctor_filter");
        params.insert("doctor_filter", doctorFilter);
    }
    if (!clinicTypeFilter.isEmpty()) {
        filters.append("(CASE WHEN doc.title LIKE '%主任%' OR doc.registration_fee >= 30 THEN '专家号' ELSE '普通号' END) = :clinic_type_filter");
        params.insert("clinic_type_filter", clinicTypeFilter);
    }
    if (!keyword.isEmpty()) {
        filters.append("(p.name LIKE CONCAT('%', :keyword_patient, '%') "
                       "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%') "
                       "OR r.registration_no LIKE CONCAT('%', :keyword_registration, '%') "
                       "OR d.dept_name LIKE CONCAT('%', :keyword_department, '%') "
                       "OR u.real_name LIKE CONCAT('%', :keyword_doctor, '%'))");
        params.insert("keyword", keyword);
        params.insert("keyword_patient", keyword);
        params.insert("keyword_id_card", keyword);
        params.insert("keyword_registration", keyword);
        params.insert("keyword_department", keyword);
        params.insert("keyword_doctor", keyword);
    }

    return SqlJson::selectRows(m_database,
        "SELECT d.dept_name AS '科室', r.registration_no AS '挂号单号', "
        "p.name AS '患者', p.id_card AS '身份证号', u.real_name AS '医生', doc.title AS '职称', "
        "CASE WHEN doc.title LIKE '%主任%' OR doc.registration_fee >= 30 THEN '专家号' ELSE '普通号' END AS '号别', "
        "CASE r.status "
        "WHEN 'CALLED' THEN '已叫号' "
        "WHEN 'IN_CONSULTATION' THEN '接诊中' "
        "WHEN 'CHECK_DONE' THEN '检查完成待复诊' "
        "ELSE r.status END AS '状态', "
        "CASE WHEN r.is_emergency = 1 THEN '急诊优先' ELSE '普通' END AS '急诊标识', "
        "COALESCE(r.emergency_reason, '') AS '急诊原因', "
        "s.work_date AS '就诊日期', COALESCE(NULLIF(r.appointment_time_slot, ''), s.period) AS '时段', "
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
        "COALESCE(GROUP_CONCAT(DISTINCT CONCAT(ex.item_name, '：', COALESCE(NULLIF(ex.report_conclusion, ''), ex.result_text)) "
        "ORDER BY ex.complete_time DESC SEPARATOR '；'), '') AS '本院检查报告', "
        "COALESCE(m.created_at, '') AS '接诊时间' "
        "FROM registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        "LEFT JOIN medical_records m ON m.registration_id = r.id "
        "LEFT JOIN examinations ex ON ex.registration_id = r.id AND ex.status = 'COMPLETED' "
        "WHERE " + filters.join(" AND ") + " "
        "GROUP BY r.id, d.dept_name, r.registration_no, p.name, p.id_card, u.real_name, doc.title, "
        "r.status, r.is_emergency, r.emergency_reason, s.work_date, s.period, r.appointment_time_slot, m.chief_complaint, m.present_illness, "
        "m.past_history, m.physical_sign, m.icd_code, m.diagnosis, m.advice, "
        "m.external_report_hospital, m.external_report_type, m.external_report_date, m.external_report_summary, "
        "m.external_report_conclusion, m.external_report_attachment, m.created_at "
        "ORDER BY CASE WHEN r.status = 'CALLED' THEN 0 WHEN r.status = 'CHECK_DONE' THEN 1 ELSE 2 END, "
        "CASE WHEN r.is_emergency = 1 THEN 0 ELSE 1 END, r.register_time DESC LIMIT 200",
        params, "consultations");
}

} // namespace hospital::server
