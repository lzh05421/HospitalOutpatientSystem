#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"
#include "server/WorkflowRules.h"

#include <QDate>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QPair>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <cmath>

namespace hospital::server {
namespace {

void releaseExpiredPendingPayments(DatabaseManager* database)
{
    if (!database || !database->ensureOpen()) {
        return;
    }

    auto db = database->database();
    if (!db.transaction()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT r.id, r.schedule_id "
                  "FROM registrations r "
                  "JOIN bills b ON b.registration_id = r.id "
                  "WHERE r.status = :pending_payment "
                  "AND b.status = 'UNPAID' "
                  "AND r.register_time < DATE_SUB(NOW(), INTERVAL 15 MINUTE)");
    query.bindValue(":pending_payment", WorkflowRules::pendingPayment());
    if (!query.exec()) {
        db.rollback();
        return;
    }

    QVector<QPair<qint64, qint64>> expiredRows;
    while (query.next()) {
        expiredRows.append({query.value(0).toLongLong(), query.value(1).toLongLong()});
    }
    query.finish();

    for (const auto& row : expiredRows) {
        QSqlQuery cancelRegistration(db);
        cancelRegistration.prepare("UPDATE registrations r "
                                   "JOIN bills b ON b.registration_id = r.id "
                                   "SET r.status = :cancelled "
                                   "WHERE r.id = :id "
                                   "AND r.status = :pending_payment "
                                   "AND b.status = 'UNPAID' "
                                   "AND r.register_time < DATE_SUB(NOW(), INTERVAL 15 MINUTE)");
        cancelRegistration.bindValue(":cancelled", WorkflowRules::cancelled());
        cancelRegistration.bindValue(":id", row.first);
        cancelRegistration.bindValue(":pending_payment", WorkflowRules::pendingPayment());
        if (!cancelRegistration.exec()) {
            db.rollback();
            return;
        }
        if (cancelRegistration.numRowsAffected() != 1) {
            cancelRegistration.finish();
            continue;
        }
        cancelRegistration.finish();

        QSqlQuery cancelBill(db);
        cancelBill.prepare("UPDATE bills SET status = 'CANCELLED' "
                           "WHERE registration_id = :registration_id AND status = 'UNPAID'");
        cancelBill.bindValue(":registration_id", row.first);
        if (!cancelBill.exec()) {
            db.rollback();
            return;
        }
        if (cancelBill.numRowsAffected() != 1) {
            db.rollback();
            return;
        }
        cancelBill.finish();

        QSqlQuery releaseQuota(db);
        releaseQuota.prepare("UPDATE doctor_schedules "
                             "SET remain_quota = LEAST(total_quota, remain_quota + 1) "
                             "WHERE id = :schedule_id");
        releaseQuota.bindValue(":schedule_id", row.second);
        if (!releaseQuota.exec()) {
            db.rollback();
            return;
        }
        releaseQuota.finish();
    }

    db.commit();
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

struct RegistrationInsuranceMessage
{
    QString code;
    QString internalStatus;
    QString patientMessage;
    QString logDescription;
};

RegistrationInsuranceMessage insuranceMessage(const QString& code)
{
    if (code == "REG_INS_001") {
        return {code, "NO_INSURANCE",
                "未查询到该参保人的医保参保信息，本次挂号无法使用医保统筹，请转为自费挂号。",
                "No registration insurance record matched patient and id card"};
    }
    if (code == "REG_INS_002") {
        return {code, "INSURANCE_EXPIRED",
                "该参保人医保有效期已过，本次挂号无法使用医保统筹，请转为自费或先办理续保。",
                "Insurance eligibility expired before registration date"};
    }
    if (code == "REG_INS_003") {
        return {code, "REMOTE_NOT_FILED",
                "该参保人异地未备案，本次挂号无法使用医保统筹，请转为自费或先办理备案。",
                "Remote insured patient has no valid remote medical filing"};
    }
    if (code == "REG_INS_004") {
        return {code, "ARREARS_SUSPENDED",
                "该参保人存在欠费停保记录，本次挂号无法使用医保统筹，请转为自费或先恢复医保待遇。",
                "Insurance benefit suspended due to arrears"};
    }
    if (code == "REG_INS_005") {
        return {code, "UNSUPPORTED_INSURANCE_TYPE",
                "该参保人当前险种不支持门诊统筹，本次挂号无法使用医保统筹，请转为自费挂号。",
                "Insurance type does not support outpatient pooling for registration"};
    }
    if (code == "REG_INS_006") {
        return {code, "ANNUAL_QUOTA_EXHAUSTED",
                "该参保人本年度门诊统筹额度已用尽，本次挂号无法使用医保统筹，请转为自费挂号。",
                "Annual outpatient pooling quota exhausted"};
    }
    if (code == "REG_INS_007") {
        return {code, "DATA_VERSION_CHANGED",
                "医保资格信息已更新，请重新进行挂号医保统筹校验后再提交。",
                "Insurance qualification data version changed between precheck and submit"};
    }
    if (code == "REG_INS_008") {
        return {code, "TOKEN_INVALID",
                "本次挂号医保校验凭证已失效，请重新选择医保统筹并完成校验。",
                "Registration insurance token missing, expired, mismatched, or already used"};
    }
    return {"REG_INS_000", "PASS",
            "医保统筹资格校验通过，本次挂号可使用医保统筹身份。",
            "Registration insurance qualification precheck passed"};
}

QString hashInsuranceToken(const QString& token)
{
    return QString::fromLatin1(QCryptographicHash::hash(token.toUtf8(),
                                                        QCryptographicHash::Sha256).toHex());
}

bool isInsurancePaymentRequested(const QJsonObject& payload)
{
    const QString value = payload.value("paymentMethod").toString().trimmed();
    return value == "医保统筹" || value == "MEDICAL_INSURANCE" || value == "INSURANCE_POOLING";
}

qint64 operatorIdFromPayload(const QJsonObject& payload)
{
    const QString operatorUserId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    if (!operatorUserId.isEmpty()) {
        return operatorUserId.toLongLong();
    }
    return 0;
}

void writeInsuranceAudit(QSqlDatabase& db,
                         const QString& stage,
                         qint64 patientId,
                         qint64 operatorId,
                         const QJsonObject& requestParams,
                         const QString& resultCode,
                         qint64 dataVersion)
{
    QSqlQuery audit(db);
    audit.prepare("INSERT INTO registration_insurance_audit_logs "
                  "(stage, patient_id, operator_id, request_params, result_code, data_version, log_description) "
                  "VALUES (:stage, :patient_id, :operator_id, :request_params, :result_code, :data_version, :log_description)");
    audit.bindValue(":stage", stage);
    audit.bindValue(":patient_id", patientId > 0 ? QVariant(patientId) : QVariant());
    audit.bindValue(":operator_id", operatorId > 0 ? QVariant(operatorId) : QVariant());
    audit.bindValue(":request_params", QString::fromUtf8(QJsonDocument(requestParams).toJson(QJsonDocument::Compact)));
    audit.bindValue(":result_code", resultCode);
    audit.bindValue(":data_version", dataVersion > 0 ? QVariant(dataVersion) : QVariant());
    audit.bindValue(":log_description", insuranceMessage(resultCode).logDescription.left(500));
    audit.exec();
    audit.finish();
}

struct InsuranceQualificationRecord
{
    bool exists = false;
    QString insuStatus;
    QDate validEndDate;
    QString insuredAreaCode;
    bool remoteFiled = false;
    int arrearsMonths = 0;
    bool benefitSuspended = false;
    QString insuranceType;
    bool outpatientSupported = false;
    double annualQuotaTotal = 0.0;
    double annualQuotaUsed = 0.0;
    int quotaYear = 0;
    qint64 dataVersion = 0;
    bool checkEnabled = false;
};

struct InsuranceSettlement
{
    double originalAmount = 0.0;
    double fullSelfPayAmount = 0.0;
    double selfPayRatioAmount = 0.0;
    double overLimitSelfPayAmount = 0.0;
    double deductible = 0.0;
    double policyScopeAmount = 0.0;
    double reimbursementRate = 0.0;
    double reimbursementAmount = 0.0;
    double selfPayAmount = 0.0;
    double annualQuotaRemaining = 0.0;
};

double roundCurrency(double value)
{
    if (value <= 0.0) {
        return 0.0;
    }
    return std::round(value * 100.0) / 100.0;
}

double outpatientRegistrationReimbursementRate(const InsuranceQualificationRecord& record)
{
    double rate = 0.0;
    if (record.insuranceType == "URBAN_EMPLOYEE") {
        rate = 0.60;
    } else if (record.insuranceType == "URBAN_RESIDENT") {
        rate = 0.50;
    }

    if (!record.insuredAreaCode.isEmpty()
        && record.insuredAreaCode != "110100"
        && record.remoteFiled) {
        rate -= 0.10;
    }
    return rate < 0.0 ? 0.0 : rate;
}

InsuranceSettlement calculateInsuranceSettlement(double totalMedicalAmount,
                                                 const InsuranceQualificationRecord& record)
{
    InsuranceSettlement settlement;
    settlement.originalAmount = roundCurrency(totalMedicalAmount);
    settlement.selfPayAmount = settlement.originalAmount;
    settlement.annualQuotaRemaining = roundCurrency(record.annualQuotaTotal - record.annualQuotaUsed);

    if (!record.exists || settlement.originalAmount <= 0.0 || settlement.annualQuotaRemaining <= 0.0) {
        return settlement;
    }

    settlement.fullSelfPayAmount = 0.0;
    settlement.selfPayRatioAmount = 0.0;
    settlement.overLimitSelfPayAmount = 0.0;
    settlement.deductible = 0.0;
    settlement.reimbursementRate = outpatientRegistrationReimbursementRate(record);
    settlement.policyScopeAmount = roundCurrency(settlement.originalAmount
                                                 - settlement.fullSelfPayAmount
                                                 - settlement.selfPayRatioAmount
                                                 - settlement.overLimitSelfPayAmount
                                                 - settlement.deductible);
    const double policyReimbursement = roundCurrency(settlement.policyScopeAmount * settlement.reimbursementRate);
    settlement.reimbursementAmount = roundCurrency(policyReimbursement > settlement.annualQuotaRemaining
                                                       ? settlement.annualQuotaRemaining
                                                       : policyReimbursement);
    settlement.selfPayAmount = roundCurrency(settlement.originalAmount - settlement.reimbursementAmount);
    return settlement;
}

InsuranceQualificationRecord loadInsuranceQualification(QSqlDatabase& db, qint64 patientId, const QString& idCard)
{
    InsuranceQualificationRecord record;
    QSqlQuery query(db);
    query.prepare("SELECT insu_status, valid_end_date, insured_area_code, is_remote_filed, "
                  "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, "
                  "annual_quota_total, annual_quota_used, quota_year, data_version, check_enabled "
                  "FROM registration_insurance_check "
                  "WHERE patient_id = :patient_id AND id_card = :id_card LIMIT 1");
    query.bindValue(":patient_id", patientId);
    query.bindValue(":id_card", idCard);
    if (query.exec() && query.next()) {
        record.exists = true;
        record.insuStatus = query.value(0).toString();
        record.validEndDate = query.value(1).toDate();
        record.insuredAreaCode = query.value(2).toString();
        record.remoteFiled = query.value(3).toInt() == 1;
        record.arrearsMonths = query.value(4).toInt();
        record.benefitSuspended = query.value(5).toInt() == 1;
        record.insuranceType = query.value(6).toString();
        record.outpatientSupported = query.value(7).toInt() == 1;
        record.annualQuotaTotal = query.value(8).toDouble();
        record.annualQuotaUsed = query.value(9).toDouble();
        record.quotaYear = query.value(10).toInt();
        record.dataVersion = query.value(11).toLongLong();
        record.checkEnabled = query.value(12).toInt() == 1;
    }
    query.finish();
    return record;
}

QString evaluateInsuranceQualification(const InsuranceQualificationRecord& record,
                                       const QString& hospitalAreaCode,
                                       const QDate& registrationDate)
{
    if (!record.exists || record.insuStatus == "NO_INSURANCE" || !record.checkEnabled) {
        return "REG_INS_001";
    }
    if (record.insuStatus == "EXPIRED"
        || (record.validEndDate.isValid() && record.validEndDate < registrationDate)) {
        return "REG_INS_002";
    }
    if (!record.insuredAreaCode.isEmpty()
        && !hospitalAreaCode.isEmpty()
        && record.insuredAreaCode != hospitalAreaCode
        && !record.remoteFiled) {
        return "REG_INS_003";
    }
    if (record.insuStatus == "ARREARS_SUSPENDED"
        || record.arrearsMonths > 0
        || record.benefitSuspended) {
        return "REG_INS_004";
    }
    if (!record.outpatientSupported
        || !(record.insuranceType == "URBAN_EMPLOYEE" || record.insuranceType == "URBAN_RESIDENT")) {
        return "REG_INS_005";
    }
    if (record.quotaYear == registrationDate.year()
        && record.annualQuotaTotal > 0.0
        && record.annualQuotaUsed >= record.annualQuotaTotal) {
        return "REG_INS_006";
    }
    return "REG_INS_000";
}

bool loadPatientIdentity(QSqlDatabase& db,
                         qint64 patientId,
                         QString* storedName,
                         QString* storedPhone,
                         QString* storedIdCard)
{
    QSqlQuery patientQuery(db);
    patientQuery.prepare("SELECT name, phone, id_card FROM patients WHERE id = :patient_id LIMIT 1");
    patientQuery.bindValue(":patient_id", patientId);
    if (!patientQuery.exec() || !patientQuery.next()) {
        patientQuery.finish();
        return false;
    }
    if (storedName) {
        *storedName = patientQuery.value(0).toString();
    }
    if (storedPhone) {
        *storedPhone = patientQuery.value(1).toString();
    }
    if (storedIdCard) {
        *storedIdCard = patientQuery.value(2).toString();
    }
    patientQuery.finish();
    return true;
}

bool patientBelongsToUser(QSqlDatabase& db, qint64 patientId, qint64 patientUserId)
{
    if (patientId <= 0 || patientUserId <= 0) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id FROM patients WHERE id = :patient_id AND user_id = :user_id LIMIT 1");
    query.bindValue(":patient_id", patientId);
    query.bindValue(":user_id", patientUserId);
    const bool belongs = query.exec() && query.next();
    query.finish();
    return belongs;
}

QString normalizedInsuranceType(const QString& value)
{
    const QString text = value.trimmed();
    if (text == "职工医保" || text == "URBAN_EMPLOYEE") {
        return "URBAN_EMPLOYEE";
    }
    if (text == "居民医保" || text == "URBAN_RESIDENT") {
        return "URBAN_RESIDENT";
    }
    if (text == "仅住院险" || text == "HOSPITAL_ONLY") {
        return "HOSPITAL_ONLY";
    }
    if (text == "工伤保险" || text == "WORK_INJURY_ONLY") {
        return "WORK_INJURY_ONLY";
    }
    return "URBAN_RESIDENT";
}

QString insuranceTypeDisplayName(const QString& value)
{
    if (value == "URBAN_EMPLOYEE") {
        return "职工医保";
    }
    if (value == "URBAN_RESIDENT") {
        return "居民医保";
    }
    if (value == "HOSPITAL_ONLY") {
        return "仅住院险";
    }
    if (value == "WORK_INJURY_ONLY") {
        return "工伤保险";
    }
    return value;
}

common::Response registrationInsuranceProfile(DatabaseManager* database, const common::Request& request)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    const qint64 patientId = request.payload.value("patientId").toVariant().toLongLong();
    const qint64 patientUserId = request.payload.value("__patientUserId").toVariant().toLongLong();
    if (patientId <= 0) {
        return {false, "请先选择就诊人后再维护医保信息。", {}};
    }
    if (patientUserId > 0 && !patientBelongsToUser(db, patientId, patientUserId)) {
        return {false, "未找到当前账号绑定的就诊人，禁止维护他人医保信息。", {}};
    }

    QString name;
    QString phone;
    QString idCard;
    if (!loadPatientIdentity(db, patientId, &name, &phone, &idCard)) {
        return {false, "未找到就诊人信息，请重新选择后再维护医保信息。", {}};
    }

    QJsonObject data;
    data["found"] = false;
    data["patientId"] = QString::number(patientId);
    data["patientName"] = name;
    data["idCard"] = idCard;
    data["hospitalAreaCode"] = "110100";

    QSqlQuery query(db);
    query.prepare("SELECT hospital_area_code, insured_area_code, valid_end_date, is_remote_filed, "
                  "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, "
                  "annual_quota_total, annual_quota_used, quota_year, data_version, check_enabled "
                  "FROM registration_insurance_check WHERE patient_id = :patient_id LIMIT 1");
    query.bindValue(":patient_id", patientId);
    if (!query.exec()) {
        return {false, query.lastError().text(), data};
    }
    if (query.next()) {
        const QString hospitalAreaCode = query.value(0).toString();
        const QString insuredAreaCode = query.value(1).toString();
        data["found"] = true;
        data["hospitalAreaCode"] = hospitalAreaCode;
        data["insuredAreaMode"] = insuredAreaCode == hospitalAreaCode ? "本地" : "异地";
        data["validEndDate"] = query.value(2).toDate().toString("yyyy-MM-dd");
        data["remoteFiled"] = query.value(3).toInt() == 1;
        data["arrearsSuspended"] = query.value(4).toInt() > 0 || query.value(5).toInt() == 1;
        data["insuranceType"] = insuranceTypeDisplayName(query.value(6).toString());
        data["outpatientSupported"] = query.value(7).toInt() == 1;
        data["annualQuotaTotal"] = query.value(8).toDouble();
        data["annualQuotaUsed"] = query.value(9).toDouble();
        data["quotaYear"] = query.value(10).toInt();
        data["dataVersion"] = QString::number(query.value(11).toLongLong());
        data["checkEnabled"] = query.value(12).toInt() == 1;
    }
    query.finish();

    return {true, data.value("found").toBool() ? "医保信息已读取。" : "未绑定医保信息，请先填写并保存。", data};
}

common::Response saveRegistrationInsuranceProfile(DatabaseManager* database, const common::Request& request)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    const qint64 patientId = request.payload.value("patientId").toVariant().toLongLong();
    const qint64 patientUserId = request.payload.value("__patientUserId").toVariant().toLongLong();
    const QString hospitalAreaCode = request.payload.value("hospitalAreaCode").toString("110100").trimmed();
    const QString insuredAreaMode = request.payload.value("insuredAreaMode").toString("本地").trimmed();
    const QString insuranceType = normalizedInsuranceType(request.payload["insuranceType"].toString());
    const QDate validEndDate = QDate::fromString(request.payload["validEndDate"].toString().trimmed(), "yyyy-MM-dd");
    const bool arrearsSuspended = request.payload.value("arrearsSuspended").toBool(false);
    const bool remoteFiled = request.payload.value("remoteFiled").toBool(false);
    const double annualQuotaUsed = request.payload["annualQuotaUsed"].toVariant().toDouble();
    const double annualQuotaTotal = request.payload["annualQuotaTotal"].toVariant().toDouble();
    const bool outpatientSupported = insuranceType == "URBAN_EMPLOYEE" || insuranceType == "URBAN_RESIDENT";

    if (patientId <= 0 || !validEndDate.isValid()) {
        return {false, "请先选择就诊人，并填写有效的医保有效期。", {}};
    }
    if (patientUserId > 0 && !patientBelongsToUser(db, patientId, patientUserId)) {
        return {false, "未找到当前账号绑定的就诊人，禁止维护他人医保信息。", {}};
    }

    QString idCard;
    if (!loadPatientIdentity(db, patientId, nullptr, nullptr, &idCard)) {
        return {false, "未找到就诊人信息，请重新选择后再维护医保信息。", {}};
    }

    const QString insuredAreaCode = insuredAreaMode == "异地" ? "310100" : hospitalAreaCode;
    const bool isRemoteFiled = insuredAreaMode == "异地" ? remoteFiled : true;
    const int arrearsMonths = arrearsSuspended ? 3 : 0;
    const QString insuStatus = arrearsSuspended ? "ARREARS_SUSPENDED"
        : (validEndDate < QDate::currentDate() ? "EXPIRED" : "ACTIVE");
    const QString expectedResultCode = evaluateInsuranceQualification(
        {true, insuStatus, validEndDate, insuredAreaCode, isRemoteFiled, arrearsMonths, arrearsSuspended,
         insuranceType, outpatientSupported, annualQuotaTotal, annualQuotaUsed, QDate::currentDate().year(), 1, true},
        hospitalAreaCode,
        QDate::currentDate());

    QSqlQuery query(db);
    query.prepare("INSERT INTO registration_insurance_check "
                  "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, "
                  "is_remote_filed, arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, "
                  "annual_quota_total, annual_quota_used, quota_year, data_version, check_enabled, expected_result_code, remark) "
                  "VALUES (:patient_id, :id_card, :hospital_area_code, :insured_area_code, :insu_status, CURRENT_DATE(), :valid_end_date, "
                  ":is_remote_filed, :arrears_months, :benefit_suspended, :insurance_type, :outpatient_pooling_supported, "
                  ":annual_quota_total, :annual_quota_used, :quota_year, 1, 1, :expected_result_code, :remark) "
                  "ON DUPLICATE KEY UPDATE "
                  "id_card = VALUES(id_card), hospital_area_code = VALUES(hospital_area_code), insured_area_code = VALUES(insured_area_code), "
                  "insu_status = VALUES(insu_status), valid_end_date = VALUES(valid_end_date), is_remote_filed = VALUES(is_remote_filed), "
                  "arrears_months = VALUES(arrears_months), benefit_suspended = VALUES(benefit_suspended), insurance_type = VALUES(insurance_type), "
                  "outpatient_pooling_supported = VALUES(outpatient_pooling_supported), annual_quota_total = VALUES(annual_quota_total), "
                  "annual_quota_used = VALUES(annual_quota_used), quota_year = VALUES(quota_year), check_enabled = 1, "
                  "expected_result_code = VALUES(expected_result_code), remark = VALUES(remark), data_version = data_version + 1");
    query.bindValue(":patient_id", patientId);
    query.bindValue(":id_card", idCard);
    query.bindValue(":hospital_area_code", hospitalAreaCode);
    query.bindValue(":insured_area_code", insuredAreaCode);
    query.bindValue(":insu_status", insuStatus);
    query.bindValue(":valid_end_date", validEndDate);
    query.bindValue(":is_remote_filed", isRemoteFiled ? 1 : 0);
    query.bindValue(":arrears_months", arrearsMonths);
    query.bindValue(":benefit_suspended", arrearsSuspended ? 1 : 0);
    query.bindValue(":insurance_type", insuranceType);
    query.bindValue(":outpatient_pooling_supported", outpatientSupported ? 1 : 0);
    query.bindValue(":annual_quota_total", annualQuotaTotal);
    query.bindValue(":annual_quota_used", annualQuotaUsed);
    query.bindValue(":quota_year", QDate::currentDate().year());
    query.bindValue(":expected_result_code", expectedResultCode);
    query.bindValue(":remark", "患者端预约挂号模块维护医保信息");
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    query.finish();

    QJsonObject data;
    data["resultCode"] = expectedResultCode;
    return {true, "医保信息已保存，请选择医保统筹进行挂号资格校验。", data};
}

common::Response registrationInsurancePrecheck(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        QJsonObject data;
        data["resultCode"] = "REG_INS_001";
        return {false, "MySQL 连接失败：" + database->lastError(), data};
    }

    auto db = database->database();
    const qint64 patientId = payload.value("patientId").toVariant().toLongLong();
    const QString hospitalAreaCode = payload.value("hospitalAreaCode").toString("110100").trimmed();
    const QDate registrationDate = QDate::fromString(payload.value("date").toString().trimmed(), "yyyy-MM-dd");
    const QString department = payload.value("department").toString().trimmed();
    const QString doctor = payload.value("doctor").toString().trimmed();
    const QString timeSlot = payload.value("timeSlot").toString().trimmed();

    QString storedIdCard;
    if (patientId <= 0 || !registrationDate.isValid() || department.isEmpty() || doctor.isEmpty() || timeSlot.isEmpty()
        || !loadPatientIdentity(db, patientId, nullptr, nullptr, &storedIdCard)) {
        QJsonObject data;
        data["resultCode"] = "REG_INS_001";
        writeInsuranceAudit(db, "PRECHECK", patientId, operatorIdFromPayload(payload), payload, "REG_INS_001", 0);
        return {false, insuranceMessage("REG_INS_001").patientMessage, data};
    }

    const auto record = loadInsuranceQualification(db, patientId, storedIdCard);
    const QString resultCode = evaluateInsuranceQualification(record, hospitalAreaCode, registrationDate);
    QJsonObject data;
    data["resultCode"] = resultCode;
    data["internalStatus"] = insuranceMessage(resultCode).internalStatus;
    data["dataVersion"] = QString::number(record.dataVersion);
    data["feePreviewEnabled"] = resultCode == "REG_INS_000";

    if (resultCode != "REG_INS_000") {
        writeInsuranceAudit(db, "PRECHECK", patientId, operatorIdFromPayload(payload), payload, resultCode, record.dataVersion);
        return {false, insuranceMessage(resultCode).patientMessage, data};
    }

    const QString insuranceToken = QUuid::createUuid().toString(QUuid::WithoutBraces)
        + "-" + QString::number(patientId)
        + "-" + QString::number(record.dataVersion);
    QSqlQuery insertToken(db);
    insertToken.prepare("INSERT INTO registration_insurance_tokens "
                        "(token_hash, patient_id, id_card, hospital_area_code, register_date, department, doctor, time_slot, "
                        "data_version, result_code, expires_at) "
                        "VALUES (:token_hash, :patient_id, :id_card, :hospital_area_code, :register_date, :department, :doctor, :time_slot, "
                        ":data_version, :result_code, DATE_ADD(NOW(), INTERVAL 5 MINUTE))");
    insertToken.bindValue(":token_hash", hashInsuranceToken(insuranceToken));
    insertToken.bindValue(":patient_id", patientId);
    insertToken.bindValue(":id_card", storedIdCard);
    insertToken.bindValue(":hospital_area_code", hospitalAreaCode);
    insertToken.bindValue(":register_date", registrationDate);
    insertToken.bindValue(":department", department);
    insertToken.bindValue(":doctor", doctor);
    insertToken.bindValue(":time_slot", timeSlot);
    insertToken.bindValue(":data_version", record.dataVersion);
    insertToken.bindValue(":result_code", resultCode);
    if (!insertToken.exec()) {
        QJsonObject failure;
        failure["resultCode"] = "REG_INS_008";
        writeInsuranceAudit(db, "PRECHECK", patientId, operatorIdFromPayload(payload), payload, "REG_INS_008", record.dataVersion);
        return {false, insuranceMessage("REG_INS_008").patientMessage, failure};
    }
    insertToken.finish();

    data["insuranceToken"] = insuranceToken;
    data["paymentIdentity"] = "医保统筹资格已通过";
    writeInsuranceAudit(db, "PRECHECK", patientId, operatorIdFromPayload(payload), payload, resultCode, record.dataVersion);
    return {true, insuranceMessage(resultCode).patientMessage, data};
}

bool validateInsuranceSubmit(QSqlDatabase& db,
                             const QJsonObject& payload,
                             qint64 patientId,
                             const QString& idCard,
                             QString* resultCode,
                             QString* resultMessage,
                             QString* tokenNo,
                             qint64* dataVersion)
{
    const QString insuranceToken = payload.value("insuranceToken").toString().trimmed();
    const QString tokenHash = hashInsuranceToken(insuranceToken);
    const QString hospitalAreaCode = payload.value("hospitalAreaCode").toString("110100").trimmed();
    const QDate registrationDate = QDate::fromString(payload.value("date").toString().trimmed(), "yyyy-MM-dd");
    const QString department = payload.value("department").toString().trimmed();
    const QString doctor = payload.value("doctor").toString().trimmed();
    const QString timeSlot = payload.value("timeSlot").toString().trimmed();

    auto fail = [&](const QString& code, qint64 version = 0) {
        if (resultCode) {
            *resultCode = code;
        }
        if (resultMessage) {
            *resultMessage = insuranceMessage(code).patientMessage;
        }
        if (dataVersion) {
            *dataVersion = version;
        }
        writeInsuranceAudit(db, "SUBMIT", patientId, operatorIdFromPayload(payload), payload, code, version);
        return false;
    };

    if (insuranceToken.isEmpty() || patientId <= 0 || idCard.isEmpty() || !registrationDate.isValid()) {
        return fail("REG_INS_008");
    }

    qint64 tokenDataVersion = 0;
    {
        QSqlQuery tokenQuery(db);
        tokenQuery.prepare("SELECT data_version FROM registration_insurance_tokens "
                           "WHERE token_hash = :token_hash "
                           "AND patient_id = :patient_id "
                           "AND id_card = :id_card "
                           "AND hospital_area_code = :hospital_area_code "
                           "AND register_date = :register_date "
                           "AND department = :department "
                           "AND doctor = :doctor "
                           "AND time_slot = :time_slot "
                           "AND expires_at > NOW() "
                           "AND used_at IS NULL "
                           "LIMIT 1");
        tokenQuery.bindValue(":token_hash", tokenHash);
        tokenQuery.bindValue(":patient_id", patientId);
        tokenQuery.bindValue(":id_card", idCard);
        tokenQuery.bindValue(":hospital_area_code", hospitalAreaCode);
        tokenQuery.bindValue(":register_date", registrationDate);
        tokenQuery.bindValue(":department", department);
        tokenQuery.bindValue(":doctor", doctor);
        tokenQuery.bindValue(":time_slot", timeSlot);
        if (!tokenQuery.exec() || !tokenQuery.next()) {
            tokenQuery.finish();
            return fail("REG_INS_008");
        }
        tokenDataVersion = tokenQuery.value(0).toLongLong();
        tokenQuery.finish();
    }

    const auto record = loadInsuranceQualification(db, patientId, idCard);
    if (!record.exists || record.dataVersion != tokenDataVersion) {
        return fail("REG_INS_007", record.dataVersion);
    }

    const QString refreshedCode = evaluateInsuranceQualification(record, hospitalAreaCode, registrationDate);
    if (refreshedCode != "REG_INS_000") {
        return fail(refreshedCode, record.dataVersion);
    }

    QSqlQuery consume(db);
    consume.prepare("UPDATE registration_insurance_tokens SET used_at = NOW() "
                    "WHERE token_hash = :token_hash AND used_at IS NULL");
    consume.bindValue(":token_hash", tokenHash);
    if (!consume.exec() || consume.numRowsAffected() != 1) {
        consume.finish();
        return fail("REG_INS_008", record.dataVersion);
    }
    consume.finish();

    if (resultCode) {
        *resultCode = "REG_INS_000";
    }
    if (resultMessage) {
        *resultMessage = insuranceMessage("REG_INS_000").patientMessage;
    }
    if (tokenNo) {
        *tokenNo = insuranceToken.left(64);
    }
    if (dataVersion) {
        *dataVersion = record.dataVersion;
    }
    writeInsuranceAudit(db, "SUBMIT", patientId, operatorIdFromPayload(payload), payload, "REG_INS_000", record.dataVersion);
    return true;
}

common::Response createRegistrationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    releaseExpiredPendingPayments(database);

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const QString patientName = payload.value("patientName").toString().trimmed();
    const QString phone = payload.value("phone").toString().trimmed();
    const QString idCard = payload.value("idCard").toString().trimmed();
    const QString doctor = payload.value("doctor").toString().trimmed();
    const QString department = payload.value("department").toString().trimmed();
    const QString date = payload.value("date").toString().trimmed();
    const QString timeSlot = payload.value("timeSlot").toString().trimmed();
    const double fee = payload.value("fee").toDouble(0.0);
    const QString operatorUserId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    const QString patientUserId = payload.value("__patientUserId").toVariant().toString().trimmed();
    const QString currentPatientId = payload.value("__patientId").toVariant().toString().trimmed();
    QString requestedPatientId = payload.value("patientId").toVariant().toString().trimmed();
    const bool isEmergency = payload.value("isEmergency").toBool(false);
    const QString emergencyReason = payload.value("emergencyReason").toString(payload.value("急诊原因").toString()).trimmed();
    const bool insuranceRequested = isInsurancePaymentRequested(payload);
    if (requestedPatientId.isEmpty()) {
        requestedPatientId = currentPatientId;
    }
    if (operatorUserId.isEmpty() && patientUserId.isEmpty()) {
        db.rollback();
        return {false, "请先登录后再挂号。", {}};
    }
    if (isEmergency && emergencyReason.isEmpty()) {
        db.rollback();
        return {false, "急诊挂号必须填写急诊原因。", {}};
    }

    qint64 patientId = 0;
    qint64 registrationUserId = patientUserId.isEmpty() ? 0 : patientUserId.toLongLong();
    bool duplicatePatient = false;
    int previousVisitCount = 0;
    if (!patientUserId.isEmpty()) {
        if (requestedPatientId.isEmpty()) {
            db.rollback();
            return {false, "请先选择就诊人后再预约。", {}};
        }
        QSqlQuery patientQuery(db);
        patientQuery.prepare("SELECT id, name, phone, id_card FROM patients "
                             "WHERE id = :patient_id AND user_id = :user_id LIMIT 1");
        patientQuery.bindValue(":patient_id", requestedPatientId.toLongLong());
        patientQuery.bindValue(":user_id", patientUserId.toLongLong());
        if (!patientQuery.exec() || !patientQuery.next()) {
            db.rollback();
            return {false, "未找到当前账号绑定的就诊人，禁止越权挂号。", {}};
        }
        patientId = patientQuery.value(0).toLongLong();
        const QString storedPatientName = patientQuery.value(1).toString();
        const QString storedPatientPhone = patientQuery.value(2).toString();
        const QString storedPatientIdCard = patientQuery.value(3).toString();
        duplicatePatient = true;
        patientQuery.finish();

        if ((!patientName.isEmpty() && patientName != storedPatientName)
            || (!phone.isEmpty() && phone != storedPatientPhone)
            || (!idCard.isEmpty() && idCard != storedPatientIdCard)) {
            db.rollback();
            return {false, "当前登录患者信息与提交信息不一致，请重新登录后再预约。", {}};
        }

        QSqlQuery countQuery(db);
        countQuery.prepare("SELECT COUNT(*) FROM registrations WHERE patient_id = :patient_id AND user_id = :user_id");
        countQuery.bindValue(":patient_id", patientId);
        countQuery.bindValue(":user_id", registrationUserId);
        if (countQuery.exec() && countQuery.next()) {
            previousVisitCount = countQuery.value(0).toInt();
        }
        countQuery.finish();
    } else if (!phone.isEmpty() || !idCard.isEmpty()) {
        QStringList patientFilters;
        if (!phone.isEmpty()) {
            patientFilters.append("phone = :phone");
        }
        if (!idCard.isEmpty()) {
            patientFilters.append("id_card = :id_card");
        }

        QSqlQuery query(db);
        query.prepare("SELECT id FROM patients WHERE " + patientFilters.join(" OR ") + " LIMIT 1");
        if (!phone.isEmpty()) {
            query.bindValue(":phone", phone);
        }
        if (!idCard.isEmpty()) {
            query.bindValue(":id_card", idCard);
        }
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }

        if (query.next()) {
            patientId = query.value(0).toLongLong();
            duplicatePatient = true;
        }
        query.finish();

        if (patientId > 0) {
            QSqlQuery countQuery(db);
            countQuery.prepare("SELECT COUNT(*) FROM registrations WHERE patient_id = :patient_id");
            countQuery.bindValue(":patient_id", patientId);
            if (countQuery.exec() && countQuery.next()) {
                previousVisitCount = countQuery.value(0).toInt();
            }
            countQuery.finish();
        }
    }

    if (patientId == 0) {
        QSqlQuery insertPatient(db);
        insertPatient.prepare("INSERT INTO patients (patient_no, name, gender, id_card, phone, address) "
                              "VALUES (:patient_no, :name, '未知', :id_card, :phone, '')");
        insertPatient.bindValue(":patient_no", "P" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
        insertPatient.bindValue(":name", patientName);
        insertPatient.bindValue(":id_card", idCard);
        insertPatient.bindValue(":phone", phone);
        if (!insertPatient.exec()) {
            db.rollback();
            return {false, insertPatient.lastError().text(), {}};
        }
        patientId = insertPatient.lastInsertId().toLongLong();
        insertPatient.finish();
    }

    QString resolvedPatientName;
    QString resolvedPatientPhone;
    QString resolvedPatientIdCard;
    if (!loadPatientIdentity(db, patientId, &resolvedPatientName, &resolvedPatientPhone, &resolvedPatientIdCard)) {
        db.rollback();
        return {false, "未找到就诊人信息，请重新选择后再挂号。", {}};
    }

    qint64 doctorId = 0;
    double registrationFee = fee;
    {
        QSqlQuery query(db);
        query.prepare("SELECT doc.id, doc.registration_fee FROM doctors doc "
                      "JOIN users u ON u.id = doc.user_id "
                      "WHERE u.real_name = :doctor AND doc.status = 1 LIMIT 1");
        query.bindValue(":doctor", doctor);
        if (!query.exec() || !query.next()) {
            db.rollback();
            return {false, "未找到可用医生。", {}};
        }
        doctorId = query.value(0).toLongLong();
        registrationFee = fee > 0 ? fee : query.value(1).toDouble();
        query.finish();
    }

    qint64 departmentId = 0;
    if (!department.isEmpty()) {
        QSqlQuery departmentQuery(db);
        departmentQuery.prepare("SELECT id FROM departments WHERE dept_name = :department LIMIT 1");
        departmentQuery.bindValue(":department", department);
        if (!departmentQuery.exec()) {
            db.rollback();
            return {false, departmentQuery.lastError().text(), {}};
        }
        if (departmentQuery.next()) {
            departmentId = departmentQuery.value(0).toLongLong();
        }
        departmentQuery.finish();
    }

    QString scheduleSql = "SELECT s.id, s.remain_quota FROM doctor_schedules s "
                          "JOIN doctors doc ON doc.id = s.doctor_id "
                          "WHERE s.doctor_id = :doctor_id AND s.work_date = :work_date "
                          "AND s.status = 1 ";
    if (departmentId > 0) {
        scheduleSql += "AND COALESCE(s.department_id, doc.department_id) = :department_id ";
    }
    scheduleSql += "ORDER BY CASE WHEN s.period = '全天' THEN 0 ELSE 1 END LIMIT 1";
    qint64 scheduleId = 0;
    int remainQuota = 0;
    {
        QSqlQuery query(db);
        query.prepare(scheduleSql);
        query.bindValue(":doctor_id", doctorId);
        query.bindValue(":work_date", date);
        if (departmentId > 0) {
            query.bindValue(":department_id", departmentId);
        }
        if (!query.exec() || !query.next()) {
            db.rollback();
            return {false, "未找到该医生当天排班，请先维护号源。", {}};
        }
        scheduleId = query.value(0).toLongLong();
        remainQuota = query.value(1).toInt();
        query.finish();
    }
    if (remainQuota <= 0) {
        db.rollback();
        return {false, "该医生当天号源已满。", {}};
    }

    QString insuranceResultCode;
    QString insuranceSubmitMessage;
    QString insuranceTokenNo;
    qint64 insuranceDataVersion = 0;
    QString paymentIdentity = "SELF_PAY";
    InsuranceQualificationRecord record;
    if (insuranceRequested) {
        if (!validateInsuranceSubmit(db,
                                     payload,
                                     patientId,
                                     resolvedPatientIdCard,
                                     &insuranceResultCode,
                                     &insuranceSubmitMessage,
                                     &insuranceTokenNo,
                                     &insuranceDataVersion)) {
            db.rollback();
            QJsonObject data;
            data["resultCode"] = insuranceResultCode;
            data["dataVersion"] = insuranceDataVersion > 0 ? QString::number(insuranceDataVersion) : QString();
            return {false, insuranceSubmitMessage, data};
        }
        record = loadInsuranceQualification(db, patientId, resolvedPatientIdCard);
        paymentIdentity = "医保统筹资格已通过";
    }
    const InsuranceSettlement settlement = calculateInsuranceSettlement(registrationFee, record);

    {
        QSqlQuery query(db);
        query.prepare("UPDATE doctor_schedules SET remain_quota = remain_quota - 1 WHERE id = :id AND remain_quota > 0");
        query.bindValue(":id", scheduleId);
        if (!query.exec() || query.numRowsAffected() != 1) {
            db.rollback();
            return {false, "号源扣减失败，请刷新后重试。", {}};
        }
        query.finish();
    }

    const QString registrationNo = "R" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    const QString billNo = "B" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    const QString paymentToken = QString::fromLatin1(QCryptographicHash::hash(
        (QUuid::createUuid().toString(QUuid::WithoutBraces)
         + QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)).toUtf8(),
        QCryptographicHash::Sha256).toHex()).left(16);
    const QString paymentTokenHash = QString::fromLatin1(QCryptographicHash::hash(paymentToken.toUtf8(),
                                                                                  QCryptographicHash::Sha256).toHex());
    qint64 registrationId = 0;
    {
        QSqlQuery query(db);
        query.prepare("INSERT INTO registrations "
                      "(registration_no, user_id, patient_id, doctor_id, schedule_id, appointment_time_slot, status, fee, "
                      "insurance_result_code, insurance_token_no, payment_identity, is_emergency, emergency_reason, operator_id) "
                      "VALUES (:registration_no, :user_id, :patient_id, :doctor_id, :schedule_id, :appointment_time_slot, :status, :fee, "
                      ":insurance_result_code, :insurance_token_no, :payment_identity, :is_emergency, :emergency_reason, :operator_id)");
        query.bindValue(":registration_no", registrationNo);
        query.bindValue(":user_id", registrationUserId > 0 ? QVariant(registrationUserId) : QVariant());
        query.bindValue(":patient_id", patientId);
        query.bindValue(":doctor_id", doctorId);
        query.bindValue(":schedule_id", scheduleId);
        query.bindValue(":appointment_time_slot", timeSlot);
        query.bindValue(":status", WorkflowRules::pendingPayment());
        query.bindValue(":fee", registrationFee);
        query.bindValue(":insurance_result_code", insuranceRequested ? QVariant(insuranceResultCode) : QVariant());
        query.bindValue(":insurance_token_no", insuranceRequested ? QVariant(insuranceTokenNo) : QVariant());
        query.bindValue(":payment_identity", paymentIdentity);
        query.bindValue(":is_emergency", isEmergency ? 1 : 0);
        query.bindValue(":emergency_reason", isEmergency ? QVariant(emergencyReason.left(255)) : QVariant());
        query.bindValue(":operator_id", operatorUserId.isEmpty() ? 1 : operatorUserId.toLongLong());
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        registrationId = query.lastInsertId().toLongLong();
        query.finish();
    }

    {
        QSqlQuery query(db);
        query.prepare("INSERT INTO bills "
                      "(bill_no, user_id, registration_id, patient_id, registration_fee, drug_fee, other_fee, total_amount, status, payment_token_hash) "
                      "VALUES (:bill_no, :user_id, :registration_id, :patient_id, :registration_fee, 0, 0, :total_amount, 'UNPAID', :payment_token_hash)");
        query.bindValue(":bill_no", billNo);
        query.bindValue(":user_id", registrationUserId > 0 ? QVariant(registrationUserId) : QVariant());
        query.bindValue(":registration_id", registrationId);
        query.bindValue(":patient_id", patientId);
        query.bindValue(":registration_fee", registrationFee);
        query.bindValue(":total_amount", settlement.selfPayAmount);
        query.bindValue(":payment_token_hash", paymentTokenHash);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        query.finish();
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data;
    data["registrationNo"] = registrationNo;
    data["status"] = WorkflowRules::pendingPayment();
    data["billNo"] = billNo;
    data["paymentToken"] = paymentToken;
    data["originalAmount"] = registrationFee;
    data["totalAmount"] = settlement.selfPayAmount;
    data["insurancePolicyScopeAmount"] = settlement.policyScopeAmount;
    data["insuranceReimbursementAmount"] = settlement.reimbursementAmount;
    data["insuranceReimbursementRate"] = settlement.reimbursementRate;
    data["paymentStatus"] = "UNPAID";
    data["insuranceResultCode"] = insuranceRequested ? insuranceResultCode : QString();
    data["paymentIdentity"] = paymentIdentity;
    data["duplicatePatient"] = duplicatePatient;
    data["previousVisitCount"] = previousVisitCount;
    const QString messagePrefix = insuranceRequested
        ? QString("医保统筹资格已通过，请前往支付。")
        : QString("请前往支付。");
    const QString message = duplicatePatient
        ? QString("%1系统识别到该手机号/身份证已有患者档案，疑似复诊或重复患者，历史就诊 %2 次。")
              .arg(messagePrefix)
              .arg(previousVisitCount)
        : messagePrefix;
    return {true, message, data};
}

common::Response updateRegistrationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    const QString nextStatus = payload.value("状态").toString().trimmed();
    if (!WorkflowRules::isValidRegistrationStatus(nextStatus)) {
        return {false, "挂号状态不合法，请通过叫号、接诊、检查或取消流程变更状态。", {}};
    }
    query.prepare("UPDATE registrations SET status = :status, fee = :fee, appointment_time_slot = :appointment_time_slot WHERE registration_no = :registration_no");
    query.bindValue(":status", nextStatus);
    query.bindValue(":fee", payload.value("挂号费").toVariant().toDouble());
    query.bindValue(":appointment_time_slot", payload.value("时段").toString());
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "挂号记录已修改。", {}};
}

common::Response cancelRegistrationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, schedule_id, status FROM registrations WHERE registration_no = :registration_no LIMIT 1");
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到要取消的挂号记录。", {}};
    }

    const qint64 registrationId = query.value(0).toLongLong();
    const qint64 scheduleId = query.value(1).toLongLong();
    const QString status = query.value(2).toString();
    query.finish();
    if (status == WorkflowRules::cancelled()) {
        db.rollback();
        return {true, "该挂号已经是取消状态。", {}};
    }
    if (!WorkflowRules::canCancelRegistration(status)) {
        db.rollback();
        return {false, "当前状态不能退号；已接诊、检查中或检查完成待复诊的记录需走病历/检查流程处理。", {}};
    }

    query.prepare("UPDATE registrations SET status = :cancelled WHERE id = :id");
    query.bindValue(":cancelled", WorkflowRules::cancelled());
    query.bindValue(":id", registrationId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE doctor_schedules SET remain_quota = LEAST(total_quota, remain_quota + 1) WHERE id = :schedule_id");
    query.bindValue(":schedule_id", scheduleId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "挂号记录已取消。", {}};
}

common::Response callRegistrationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE registrations SET status = :called "
                  "WHERE registration_no = :registration_no "
                  "AND (status = :waiting OR status = :check_done)");
    query.bindValue(":called", WorkflowRules::called());
    query.bindValue(":registration_no", payload.value("挂号单号").toString());
    query.bindValue(":waiting", WorkflowRules::waiting());
    query.bindValue(":check_done", WorkflowRules::checkDone());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        return {false, "只有待叫号或检查完成待复诊的患者可以叫号。", {}};
    }
    return {true, "叫号成功，患者状态已更新。", {}};
}

common::Response markEmergencyRegistrationInDatabase(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString emergencyReason = payload.value("急诊原因").toString(payload.value("emergencyReason").toString()).trimmed();
    if (registrationNo.isEmpty()) {
        return {false, "请选择要设置急诊优先的候诊患者。", {}};
    }
    if (emergencyReason.isEmpty()) {
        return {false, "请填写急诊原因。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE registrations "
                  "SET is_emergency = 1, emergency_reason = :emergency_reason "
                  "WHERE registration_no = :registration_no "
                  "AND status IN (:waiting, :called, :check_done)");
    query.bindValue(":emergency_reason", emergencyReason.left(255));
    query.bindValue(":registration_no", registrationNo);
    query.bindValue(":waiting", WorkflowRules::waiting());
    query.bindValue(":called", WorkflowRules::called());
    query.bindValue(":check_done", WorkflowRules::checkDone());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        return {false, "只有候诊、已叫号或检查完成待复诊的患者可以设置急诊优先。", {}};
    }
    return {true, "已设置急诊优先，候诊队列将优先显示。", {}};
}

common::Response waitingQueueInDatabase(DatabaseManager* database, const common::Request& request)
{
    releaseExpiredPendingPayments(database);

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    const QString departmentFilter = request.payload.value("departmentFilter").toString().trimmed();
    const QString doctorFilter = request.payload.value("doctorFilter").toString().trimmed();
    const QString clinicTypeFilter = request.payload.value("clinicTypeFilter").toString().trimmed();
    const QString operatorRoleCode = request.payload.value("__operatorRoleCode").toString().trimmed().toUpper();
    const QString operatorName = request.payload.value("__operatorName").toString().trimmed();
    const QString operatorDoctorId = request.payload.value("__doctorId").toVariant().toString().trimmed();
    const bool operatorIsDoctor = operatorRoleCode == "DOCTOR";

    QStringList filters = {"r.status IN ('WAITING', 'CALLED', 'CHECK_DONE')"};
    QVariantMap params;
    applyVisitDateFilter(filters, params, request.payload);
    if (operatorIsDoctor) {
        if (operatorDoctorId.isEmpty()) {
            return {false, "当前医生账号未绑定医生档案，请在医生管理中关联该账号。", {}};
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

    return SqlJson::selectRows(database,
        "SELECT d.dept_name AS '科室', u.real_name AS '医生', doc.title AS '职称', "
        "CASE WHEN doc.title LIKE '%主任%' OR doc.registration_fee >= 30 THEN '专家号' ELSE '普通号' END AS '号别', "
        "p.name AS '患者', p.id_card AS '身份证号', r.registration_no AS '挂号单号', s.work_date AS '就诊日期', "
        "COALESCE(NULLIF(r.appointment_time_slot, ''), s.period) AS '时段', "
        "CASE r.status "
        "WHEN 'WAITING' THEN '待叫号' "
        "WHEN 'CALLED' THEN '已叫号' "
        "WHEN 'CHECK_DONE' THEN '检查完成待复诊' "
        "ELSE r.status END AS '候诊状态', "
        "CASE WHEN r.is_emergency = 1 THEN '急诊优先' ELSE '普通' END AS '急诊标识', "
        "COALESCE(r.emergency_reason, '') AS '急诊原因', "
        "(SELECT COUNT(*) FROM registrations r2 "
        "JOIN doctor_schedules s2 ON s2.id = r2.schedule_id "
        "WHERE r2.status IN ('WAITING', 'CALLED', 'CHECK_DONE') "
        "AND r2.doctor_id = r.doctor_id AND s2.work_date = s.work_date "
        "AND (CASE WHEN r2.is_emergency = 1 THEN 0 ELSE 1 END) <= (CASE WHEN r.is_emergency = 1 THEN 0 ELSE 1 END) "
        "AND r2.register_time <= r.register_time) AS '排队序号', "
        "CONCAT(GREATEST((SELECT COUNT(*) FROM registrations r3 "
        "JOIN doctor_schedules s3 ON s3.id = r3.schedule_id "
        "WHERE r3.status IN ('WAITING', 'CALLED', 'CHECK_DONE') "
        "AND r3.doctor_id = r.doctor_id AND s3.work_date = s.work_date "
        "AND (CASE WHEN r3.is_emergency = 1 THEN 0 ELSE 1 END) <= (CASE WHEN r.is_emergency = 1 THEN 0 ELSE 1 END) "
        "AND r3.register_time < r.register_time) * 8, 0), '分钟') AS '预计等待', "
        "r.register_time AS '挂号时间' "
        "FROM registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        "WHERE " + filters.join(" AND ") + " "
        "ORDER BY s.work_date, s.period, CASE WHEN r.is_emergency = 1 THEN 0 ELSE 1 END, r.register_time LIMIT 200",
        params, "waitingQueue");
}

common::Response orderHistoryInDatabase(DatabaseManager* database, const common::Request& request)
{
    releaseExpiredPendingPayments(database);

    const QString patientUserId = request.payload.value("__patientUserId").toVariant().toString().trimmed();
    QString patientId = request.payload.value("patientId").toVariant().toString().trimmed();
    if (patientId.isEmpty()) {
        patientId = request.payload.value("__patientId").toVariant().toString().trimmed();
    }
    const QString operatorUserId = request.payload.value("__operatorUserId").toVariant().toString().trimmed();
    if (patientUserId.isEmpty() && patientId.isEmpty() && operatorUserId.isEmpty()) {
        return {false, "请先登录后再查看历史订单。", {}};
    }

    QVariantMap params;
    QStringList scopes;
    if (!patientUserId.isEmpty()) {
        scopes.append("r.user_id = :user_id");
        params.insert("user_id", patientUserId.toLongLong());
        if (!patientId.isEmpty()) {
            scopes.append("r.patient_id = :patient_id");
            params.insert("patient_id", patientId.toLongLong());
        }
    } else if (!patientId.isEmpty()) {
        scopes.append("r.patient_id = :patient_id");
        params.insert("patient_id", patientId.toLongLong());
    } else {
        scopes.append("r.operator_id = :operator_user_id");
        params.insert("operator_user_id", operatorUserId.toLongLong());
    }
    const QString endpoint = "GET /api/appointments/my-history?patientId={currentUserId}";
    Q_UNUSED(endpoint);
    return SqlJson::selectRows(database,
        "SELECT r.registration_no AS 'registrationNo', p.name AS 'patientName', d.dept_name AS 'department', "
        "u.real_name AS 'doctorName', s.work_date AS 'visitDate', COALESCE(NULLIF(r.appointment_time_slot, ''), s.period) AS 'timeSlot', "
        "CASE r.status WHEN 'PENDING_PAYMENT' THEN '待支付' "
        "WHEN 'CANCELLED' THEN '已取消' ELSE '已完成' END AS 'status', "
        "r.payment_identity AS 'paymentIdentity', r.insurance_result_code AS 'insuranceResultCode', "
        "b.bill_no AS 'billNo', b.id AS 'billId', b.status AS 'paymentStatus', "
        "b.total_amount AS 'totalAmount', r.register_time AS 'createdAt' "
        "FROM registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        "LEFT JOIN bills b ON b.registration_id = r.id "
        "WHERE " + scopes.join(" AND ") + " "
        "ORDER BY r.register_time DESC LIMIT 100",
        params, "registrationHistory");
}

} // namespace

RegistrationService::RegistrationService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response RegistrationService::handle(const common::Request& request)
{
    if (request.action == "insuranceProfile") {
        if (m_database->isEnabled()) {
            return registrationInsuranceProfile(m_database, request);
        }
        QJsonObject data;
        data["found"] = false;
        return {true, "未绑定医保信息，请先填写并保存。", data};
    }
    if (request.action == "saveInsuranceProfile") {
        if (m_database->isEnabled()) {
            return saveRegistrationInsuranceProfile(m_database, request);
        }
        return {false, "医保信息维护需要连接本地仿真数据库。", {}};
    }
    if (request.action == "insurancePrecheck") {
        if (m_database->isEnabled()) {
            return registrationInsurancePrecheck(m_database, request.payload);
        }
        QJsonObject data;
        data["resultCode"] = "REG_INS_001";
        return {false, "医保资格校验需要连接本地仿真数据库。", data};
    }
    if (request.action == "create") {
        if (m_database->isEnabled()) {
            return createRegistrationInDatabase(m_database, request.payload);
        }

        const auto result = DemoRepository::instance().addRegistration(request.payload);
        return {result.value("success").toBool(), result.value("message").toString(), result};
    }
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updateRegistration(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateRegistrationInDatabase(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().cancelRegistration(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return cancelRegistrationInDatabase(m_database, request.payload);
    }
    if (request.action == "call") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().callRegistration(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return callRegistrationInDatabase(m_database, request.payload);
    }
    if (request.action == "markEmergency") {
        if (!m_database->isEnabled()) {
            return {true, "Demo 模式已设置急诊优先。", {}};
        }
        return markEmergencyRegistrationInDatabase(m_database, request.payload);
    }
    if (request.action == "waiting") {
        const QString keyword = request.payload.value("keyword").toString().trimmed();
        const QString departmentFilter = request.payload.value("departmentFilter").toString().trimmed();
        const QString doctorFilter = request.payload.value("doctorFilter").toString().trimmed();
        const QString clinicTypeFilter = request.payload.value("clinicTypeFilter").toString().trimmed();
        if (!m_database->isEnabled()) {
            const auto rows = DemoRepository::instance().waitingQueue(keyword, departmentFilter, doctorFilter, clinicTypeFilter);
            QJsonObject data;
            data["rows"] = rows;
            data["count"] = rows.size();
            return {true, "Demo data", data};
        }
        return waitingQueueInDatabase(m_database, request);
    }
    if (request.action == "history") {
        if (!m_database->isEnabled()) {
            QJsonObject data;
            data["rows"] = QJsonArray();
            data["count"] = 0;
            data["registrationHistory"] = true;
            return {true, "Demo history", data};
        }
        return orderHistoryInDatabase(m_database, request);
    }

    if (request.action != "list") {
        return {false, "Unsupported registration action", {}};
    }

    if (m_database->isEnabled()) {
        releaseExpiredPendingPayments(m_database);
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    QStringList filters;
    QVariantMap params;
    if (!keyword.isEmpty()) {
        filters.append("(p.name LIKE CONCAT('%', :keyword_patient, '%') "
                       "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%') "
                       "OR r.registration_no LIKE CONCAT('%', :keyword_registration, '%') "
                       "OR d.dept_name LIKE CONCAT('%', :keyword_department, '%') "
                       "OR u.real_name LIKE CONCAT('%', :keyword_doctor, '%'))");
        params.insert("keyword_patient", keyword);
        params.insert("keyword_id_card", keyword);
        params.insert("keyword_registration", keyword);
        params.insert("keyword_department", keyword);
        params.insert("keyword_doctor", keyword);
    }
    const QString whereClause = filters.isEmpty() ? QString() : QString("WHERE %1 ").arg(filters.join(" AND "));

    return SqlJson::selectRows(m_database,
        "SELECT r.registration_no AS '挂号单号', p.name AS '患者', p.id_card AS '身份证号', d.dept_name AS '科室', "
        "u.real_name AS '医生', s.work_date AS '就诊日期', COALESCE(NULLIF(r.appointment_time_slot, ''), s.period) AS '时段', "
        "CASE r.status WHEN 'PENDING_PAYMENT' THEN '待支付' "
        "WHEN 'WAITING' THEN '待叫号' WHEN 'CHECKING' THEN '检查中' "
        "WHEN 'CALLED' THEN '已叫号' "
        "WHEN 'IN_CONSULTATION' THEN '接诊中' WHEN 'CHECK_DONE' THEN '检查完成待复诊' "
        "WHEN 'FINISHED' THEN '已接诊' WHEN 'CANCELLED' THEN '已取消' ELSE r.status END AS '状态', "
        "CASE WHEN r.is_emergency = 1 THEN '急诊优先' ELSE '普通' END AS '急诊标识', "
        "COALESCE(r.emergency_reason, '') AS '急诊原因', "
        "r.fee AS '挂号费', r.register_time AS '挂号时间' "
        "FROM registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "JOIN doctors doc ON doc.id = r.doctor_id "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "JOIN departments d ON d.id = COALESCE(s.department_id, doc.department_id) "
        + whereClause +
        "ORDER BY r.register_time DESC LIMIT 100",
        params, "registrations");
}

} // namespace hospital::server
