#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"
#include "server/WorkflowRules.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVariant>

#include <cmath>
#include <stdexcept>

namespace hospital::server {
namespace {

constexpr auto kInsuranceProcessingStatus = "INSURANCE_PROCESSING";
constexpr auto kInsuranceCallbackSecret = "mock-insurance-callback-secret";

bool isPendingBillStatus(const QString& status)
{
    return status == "UNPAID" || status == "PENDING" || status == WorkflowRules::pendingPayment();
}

QString paymentNo(const QString& prefix = "PAY")
{
    return prefix + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
}

QString refundNo()
{
    return "RF" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
}

qint64 operatorUserIdFromPayload(const QJsonObject& payload)
{
    const QString operatorId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    return operatorId.isEmpty() ? 1 : operatorId.toLongLong();
}

bool appendBillingOperationLog(QSqlDatabase& db,
                               qint64 userId,
                               const QString& action,
                               const QString& content,
                               QString* error)
{
    QSqlQuery log(db);
    log.prepare("INSERT INTO operation_logs (user_id, module, action, content) "
                "VALUES (:user_id, 'billing', :action, :content)");
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

QJsonObject paidResponseData(const QString& billNo, double amount)
{
    QJsonObject data;
    data["billNo"] = billNo;
    data["registrationStatus"] = WorkflowRules::waiting();
    data["paymentStatus"] = "PAID";
    data["totalAmount"] = amount;
    return data;
}

QJsonArray billItems(double registrationFee, double drugFee, double otherFee)
{
    QJsonArray items;
    if (registrationFee > 0.0) {
        items.append(QJsonObject{{"name", "registration_fee"}, {"amount", registrationFee}});
    }
    if (drugFee > 0.0) {
        items.append(QJsonObject{{"name", "drug_fee"}, {"amount", drugFee}});
    }
    if (otherFee > 0.0) {
        items.append(QJsonObject{{"name", "other_fee"}, {"amount", otherFee}});
    }
    return items;
}

bool bindCashier(QSqlQuery& query, const QJsonObject& payload)
{
    const QString cashierId = payload.value("__operatorUserId").toVariant().toString();
    query.bindValue(":cashier_id", cashierId.isEmpty() ? 1 : cashierId.toLongLong());
    return true;
}

QString tokenHash(const QString& token)
{
    return QString::fromLatin1(QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool hasTrustedOperator(const QJsonObject& payload)
{
    return !payload.value("__operatorUserId").toVariant().toString().trimmed().isEmpty();
}

bool isValidPatientPaymentToken(const QJsonObject& payload, const QString& storedHash)
{
    const QString token = payload.value("paymentToken").toString().trimmed();
    return !storedHash.isEmpty() && !token.isEmpty() && tokenHash(token) == storedHash;
}

bool hasBillingWideScope(const QJsonObject& payload)
{
    const QString role = payload.value("__operatorRoleCode").toString().toUpper();
    return role == "ADMIN" || role == "CASHIER";
}

QString signedQrPayload(const QString& billNo, qint64 billId, const QString& operatorUserId)
{
    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString body = QString("BillID=%1&billNo=%2&userId=%3&nonce=%4")
        .arg(billId)
        .arg(billNo, operatorUserId, nonce);
    const QString signature = QString::fromLatin1(QCryptographicHash::hash(
        (body + "|hospital-payment-signing-key").toUtf8(), QCryptographicHash::Sha256).toHex());
    return "hospital-pay://" + body + "&signature=" + signature;
}

QString qrSvgBase64(const QString& payload)
{
    const QByteArray hash = QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha256).toHex();
    QString cells;
    int index = 0;
    for (int y = 0; y < 21; ++y) {
        for (int x = 0; x < 21; ++x) {
            const bool finder = (x < 7 && y < 7) || (x > 13 && y < 7) || (x < 7 && y > 13);
            const bool dark = finder || (hash.at(index++ % hash.size()) % 3 == 0);
            if (dark) {
                cells += QString("<rect x=\"%1\" y=\"%2\" width=\"8\" height=\"8\"/>").arg(x * 8).arg(y * 8);
            }
        }
    }
    const QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"192\" height=\"192\" viewBox=\"0 0 168 168\">"
                                "<rect width=\"168\" height=\"168\" fill=\"white\"/><g fill=\"#111827\">%1</g></svg>")
        .arg(cells);
    return QString::fromLatin1(svg.toUtf8().toBase64());
}

bool assertBillVisibleToSession(DatabaseManager* database,
                                const QString& billNo,
                                const QJsonObject& payload,
                                QJsonObject* bill,
                                QString* error)
{
    const QString operatorUserId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    const QString patientUserId = payload.value("__patientUserId").toVariant().toString().trimmed();
    const QString patientId = payload.value("__patientId").toVariant().toString().trimmed();
    if (operatorUserId.isEmpty() && patientUserId.isEmpty() && patientId.isEmpty()) {
        if (error) {
            *error = "请先登录后再操作账单。";
        }
        return false;
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT b.id, b.bill_no, b.total_amount, b.status, r.status, r.operator_id "
                  "FROM bills b "
                  "JOIN registrations r ON r.id = b.registration_id "
                  "WHERE b.bill_no = :bill_no "
                  "AND (:can_view_all = 1 OR r.operator_id = :operator_user_id "
                  "OR b.user_id = :user_id OR r.user_id = :user_id OR b.patient_id = :patient_id) "
                  "LIMIT 1");
    query.bindValue(":bill_no", billNo);
    query.bindValue(":can_view_all", hasBillingWideScope(payload) ? 1 : 0);
    query.bindValue(":operator_user_id", operatorUserId.toLongLong());
    query.bindValue(":user_id", patientUserId.toLongLong());
    query.bindValue(":patient_id", patientId.toLongLong());
    if (!query.exec()) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    if (!query.next()) {
        if (error) {
            *error = "未找到当前用户可访问的账单，禁止越权查询或支付。";
        }
        return false;
    }
    if (bill) {
        (*bill)["billId"] = QString::number(query.value(0).toLongLong());
        (*bill)["billNo"] = query.value(1).toString();
        (*bill)["totalAmount"] = query.value(2).toDouble();
        (*bill)["paymentStatus"] = query.value(3).toString();
        (*bill)["registrationStatus"] = query.value(4).toString();
        (*bill)["operatorUserId"] = QString::number(query.value(5).toLongLong());
    }
    return true;
}

} // namespace

BillingService::BillingService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response BillingService::updateBill(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery query(m_database->database());
    const QString status = payload.value("状态").toString().trimmed();
    if (status == "PAID" || status == "REFUNDED" || status == "CANCELLED" || status == kInsuranceProcessingStatus
        || status == "已缴费" || status == "已退费" || status == "已取消") {
        return {false, "账单状态请通过收费、退费或取消流程变更，不能直接编辑。", {}};
    }
    query.prepare("UPDATE bills SET registration_fee = :registration_fee, drug_fee = :drug_fee, "
                  "other_fee = :other_fee, total_amount = :total_amount, status = :status "
                  "WHERE bill_no = :bill_no");
    query.bindValue(":registration_fee", payload.value("挂号费").toVariant().toDouble());
    query.bindValue(":drug_fee", payload.value("药品费").toVariant().toDouble());
    query.bindValue(":other_fee", payload.value("其他费用").toVariant().toDouble());
    query.bindValue(":total_amount", payload.value("合计").toVariant().toDouble());
    query.bindValue(":status", status);
    query.bindValue(":bill_no", payload.value("账单号").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "账单信息已修改。", {}};
}

common::Response BillingService::cancelBill(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery query(m_database->database());
    query.prepare("UPDATE bills SET status = 'CANCELLED' WHERE bill_no = :bill_no");
    query.bindValue(":bill_no", payload.value("账单号").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "账单已取消。", {}};
}

common::Response BillingService::processSelfPay(const QJsonObject& payload)
{
    const QString traceBillNo = payload.value("账单号").toString(payload.value("billNo").toString()).trimmed();
    qInfo().noquote() << "BillingRequest action=pay billNo=" + traceBillNo;
    if (!m_database->ensureOpen()) {
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false error=" + m_database->lastError();
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    auto db = m_database->database();
    if (!db.transaction()) {
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false error=" + db.lastError().text();
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT b.id, b.patient_id, b.total_amount, b.status, b.user_id, r.id, r.status, b.payment_token_hash, r.operator_id "
                  "FROM bills b "
                  "LEFT JOIN registrations r ON r.id = b.registration_id "
                  "WHERE b.bill_no = :bill_no LIMIT 1");
    query.bindValue(":bill_no", payload.value("账单号").toString());
    if (!query.exec() || !query.next()) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false error=missing_bill";
        return {false, "未找到要收费的账单。", {}};
    }

    const qint64 billId = query.value(0).toLongLong();
    const qint64 patientId = query.value(1).toLongLong();
    const double amount = query.value(2).toDouble();
    const QString status = query.value(3).toString();
    const qint64 billUserId = query.value(4).toLongLong();
    const qint64 registrationId = query.value(5).toLongLong();
    const QString registrationStatus = query.value(6).toString();
    const QString paymentTokenHash = query.value(7).toString();
    const qint64 operatorId = query.value(8).toLongLong();
    query.finish();

    const QString operatorUserId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    const QString patientUserId = payload.value("__patientUserId").toVariant().toString().trimmed();
    const QString currentPatientId = payload.value("__patientId").toVariant().toString().trimmed();
    const bool ownsBill = operatorId == operatorUserId.toLongLong()
        || billUserId == patientUserId.toLongLong()
        || patientId == currentPatientId.toLongLong();
    const bool validPaymentToken = isValidPatientPaymentToken(payload, paymentTokenHash);
    if (!hasBillingWideScope(payload) && !ownsBill && !validPaymentToken) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false error=forbidden";
        return {false, "未找到当前用户可访问的账单，禁止越权支付。", {}};
    }
    if (!hasTrustedOperator(payload) && !ownsBill && !validPaymentToken) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false error=invalid_payment_token";
        return {false, "支付凭证无效，请重新预约或联系收费窗口处理。", {}};
    }

    const bool isMockPay = payload.value("__mockPay").toBool(false);
    const QJsonValue clientAmount = payload.value("合计").isUndefined()
        ? payload.value("amount")
        : payload.value("合计");
    if (!isMockPay && (clientAmount.isUndefined() || clientAmount.isNull())) {
        db.rollback();
        return {false, "支付金额缺失，请刷新账单后重试。", {}};
    }
    if (!isMockPay && !clientAmount.isUndefined() && !clientAmount.isNull()) {
        const double submittedAmount = clientAmount.toVariant().toDouble();
        if (std::fabs(submittedAmount - amount) > 0.005) {
            db.rollback();
            qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                                 << "success=false error=amount_mismatch";
            return {false, "支付金额校验失败，请刷新账单后重试。", {}};
        }
    }

    if (status == "PAID") {
        db.rollback();
        qInfo().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                          << "success=true already_paid";
        return {true, "该账单已缴费。", paidResponseData(payload.value("账单号").toString(), amount)};
    }
    if (status == "REFUNDED" || status == "CANCELLED") {
        db.rollback();
        return {false, "已退费或已取消的账单不能再次收费。", {}};
    }
    if (status == kInsuranceProcessingStatus) {
        db.rollback();
        return {false, "该账单正在等待医保支付结果，请勿重复发起自费支付。", {}};
    }
    if (!isPendingBillStatus(status)) {
        db.rollback();
        return {false, QString("当前账单状态为 %1，不能直接支付。").arg(status), {}};
    }
    if (registrationId <= 0) {
        db.rollback();
        return {false, "账单未关联挂号记录，不能完成挂号支付。", {}};
    }
    if (registrationStatus != WorkflowRules::pendingPayment()
        && registrationStatus != WorkflowRules::waiting()) {
        db.rollback();
        return {false, QString("当前挂号状态为 %1，不能通过收费进入候诊。").arg(registrationStatus), {}};
    }

    query.prepare("UPDATE bills SET status = 'PAID', pay_time = NOW(), payment_token_hash = NULL "
                  "WHERE id = :id AND status IN ('UNPAID', 'PENDING', 'PENDING_PAYMENT')");
    query.bindValue(":id", billId);
    if (!query.exec()) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false step=update_bill error=" + query.lastError().text();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() != 1) {
        QSqlQuery statusCheck(db);
        statusCheck.prepare("SELECT status FROM bills WHERE id = :id");
        statusCheck.bindValue(":id", billId);
        const bool statusLoaded = statusCheck.exec() && statusCheck.next();
        const QString currentStatus = statusLoaded ? statusCheck.value(0).toString() : QString();
        statusCheck.finish();
        db.rollback();
        if (currentStatus == "PAID") {
            return {true, "该账单已缴费。", paidResponseData(payload.value("账单号").toString(), amount)};
        }
        if (currentStatus == "CANCELLED") {
            return {false, "该待支付订单已超时取消，请重新预约。", {}};
        }
        return {false, "账单状态已变化，请刷新后重试支付。", {}};
    }

    query.prepare("UPDATE registrations SET status = :waiting "
                  "WHERE id = :id AND status = :pending_payment");
    query.bindValue(":waiting", WorkflowRules::waiting());
    query.bindValue(":id", registrationId);
    query.bindValue(":pending_payment", WorkflowRules::pendingPayment());
    if (!query.exec()) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false step=update_registration error=" + query.lastError().text();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() != 1 && registrationStatus == WorkflowRules::pendingPayment()) {
        db.rollback();
        return {false, "挂号状态已变化，请刷新后重试支付。", {}};
    }

    QSqlQuery payment(db);
    payment.prepare("INSERT INTO payments (payment_no, bill_id, amount, pay_method, cashier_id) "
                    "VALUES (:payment_no, :bill_id, :amount, :pay_method, :cashier_id)");
    payment.bindValue(":payment_no", payload.value("发票号").toString(
        paymentNo()));
    payment.bindValue(":bill_id", billId);
    payment.bindValue(":amount", amount);
    payment.bindValue(":pay_method", payload.value("支付方式").toString("患者自助支付"));
    bindCashier(payment, payload);
    if (!payment.exec()) {
        db.rollback();
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false step=insert_payment error=" + payment.lastError().text();
        return {false, payment.lastError().text(), {}};
    }

    if (!db.commit()) {
        qWarning().noquote() << "BillingResponse action=pay billNo=" + traceBillNo
                             << "success=false step=commit error=" + db.lastError().text();
        return {false, db.lastError().text(), {}};
    }
    qInfo().noquote() << "BillingResponse action=pay billNo=" + traceBillNo << "success=true";
    return {true, "支付成功，患者已进入候诊。", paidResponseData(payload.value("账单号").toString(), amount)};
}

BillingService::InsuranceApiResult BillingService::callMedicalInsuranceAPI(const QString& transactionId,
                                                                            qint64 billId,
                                                                            qint64 patientId,
                                                                            double totalAmount,
                                                                            const QJsonArray& items)
{
    if (transactionId.isEmpty() || billId <= 0 || patientId <= 0 || totalAmount <= 0.0 || items.isEmpty()) {
        throw std::runtime_error("医保请求参数不完整");
    }

    InsuranceApiResult result;
    result.accepted = true;
    result.transactionId = transactionId;
    result.message = "本地医保仿真请求已受理。";
    return result;
}

void BillingService::logPaymentError(const QString& billNo,
                                     const QString& action,
                                     const QString& message,
                                     const QString& operatorId) const
{
    if (!m_database || !m_database->isEnabled() || !m_database->ensureOpen()) {
        return;
    }

    QSqlQuery log(m_database->database());
    log.prepare("INSERT INTO operation_logs (user_id, module, action, content) "
                "VALUES (:user_id, 'billing', :action, :content)");
    log.bindValue(":user_id", operatorId.isEmpty() ? QVariant() : QVariant(operatorId.toLongLong()));
    log.bindValue(":action", action.left(64));
    log.bindValue(":content", QString("账单 %1：%2").arg(billNo, message).left(500));
    log.exec();
}

common::Response BillingService::processMedicalInsurancePay(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString billNo = payload.value("账单号").toString().trimmed();
    auto db = m_database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT b.id, b.registration_id, b.patient_id, b.user_id, b.total_amount, b.status, b.payment_token_hash, "
                  "b.registration_fee, b.drug_fee, b.other_fee, r.operator_id, r.status "
                  "FROM bills b "
                  "JOIN registrations r ON r.id = b.registration_id "
                  "WHERE b.bill_no = :bill_no LIMIT 1");
    query.bindValue(":bill_no", billNo);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到要医保支付的账单。", {}};
    }

    const qint64 billId = query.value(0).toLongLong();
    const qint64 registrationId = query.value(1).toLongLong();
    const qint64 patientId = query.value(2).toLongLong();
    const qint64 billUserId = query.value(3).toLongLong();
    const double amount = query.value(4).toDouble();
    const QString status = query.value(5).toString();
    const QString paymentTokenHash = query.value(6).toString();
    const qint64 operatorId = query.value(10).toLongLong();
    const QString registrationStatus = query.value(11).toString();
    query.finish();

    if (!hasTrustedOperator(payload) && !isValidPatientPaymentToken(payload, paymentTokenHash)) {
        db.rollback();
        return {false, "支付凭证无效，请重新预约或联系收费窗口处理。", {}};
    }
    const QString operatorUserId = payload.value("__operatorUserId").toVariant().toString().trimmed();
    const QString patientUserId = payload.value("__patientUserId").toVariant().toString().trimmed();
    const QString currentPatientId = payload.value("__patientId").toVariant().toString().trimmed();
    if (!hasBillingWideScope(payload)
        && operatorId != operatorUserId.toLongLong()
        && billUserId != patientUserId.toLongLong()
        && patientId != currentPatientId.toLongLong()) {
        db.rollback();
        return {false, "未找到当前用户可访问的账单，禁止越权支付。", {}};
    }

    if (status == "PAID") {
        db.rollback();
        return {true, "该账单已缴费。", paidResponseData(billNo, amount)};
    }
    if (!isPendingBillStatus(status) && status != kInsuranceProcessingStatus) {
        db.rollback();
        return {false, QString("当前账单状态为 %1，不能发起医保支付。").arg(status), {}};
    }

    const QString transactionId = "MI" + QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmsszzz")
        + QString::number(billId);
    QSqlQuery tx(db);
    tx.prepare("UPDATE bills SET status = 'PAID' "
               "WHERE id = :id AND status IN ('UNPAID', 'PENDING', 'PENDING_PAYMENT', 'INSURANCE_PROCESSING')");
    tx.bindValue(":id", billId);
    if (!tx.exec()) {
        db.rollback();
        return {false, tx.lastError().text(), {}};
    }
    if (tx.numRowsAffected() != 1) {
        db.rollback();
        return {false, "账单状态已变化，本地医保仿真支付未完成，请刷新后重试。", {}};
    }

    tx.prepare("INSERT INTO insurance_transactions "
               "(transaction_id, bill_id, patient_id, amount, status, request_payload) "
               "VALUES (:transaction_id, :bill_id, :patient_id, :amount, 'SUCCESS', :request_payload)");
    tx.bindValue(":transaction_id", transactionId);
    tx.bindValue(":bill_id", billId);
    tx.bindValue(":patient_id", patientId);
    tx.bindValue(":amount", amount);
    tx.bindValue(":request_payload", QString::fromUtf8(QJsonDocument(QJsonObject{
        {"billId", billId},
        {"patientId", patientId},
        {"totalAmount", amount},
        {"localSimulation", true}
    }).toJson(QJsonDocument::Compact)));
    if (!tx.exec()) {
        db.rollback();
        return {false, tx.lastError().text(), {}};
    }

    tx.prepare("UPDATE registrations SET status = :waiting "
               "WHERE id = :id AND status = :pending_payment");
    tx.bindValue(":waiting", WorkflowRules::waiting());
    tx.bindValue(":id", registrationId);
    tx.bindValue(":pending_payment", WorkflowRules::pendingPayment());
    if (!tx.exec()) {
        db.rollback();
        return {false, tx.lastError().text(), {}};
    }
    if (tx.numRowsAffected() != 1 && registrationStatus == WorkflowRules::pendingPayment()) {
        db.rollback();
        return {false, "挂号状态已变化，请刷新后重试支付。", {}};
    }

    tx.prepare("INSERT INTO payments (payment_no, bill_id, amount, pay_method, cashier_id) "
               "VALUES (:payment_no, :bill_id, :amount, '医保统筹（本地仿真）', :cashier_id)");
    tx.bindValue(":payment_no", paymentNo("MIPAY"));
    tx.bindValue(":bill_id", billId);
    tx.bindValue(":amount", amount);
    bindCashier(tx, payload);
    if (!tx.exec()) {
        db.rollback();
        return {false, tx.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data = paidResponseData(billNo, amount);
    data["transactionId"] = transactionId;
    data["simulationMode"] = "LOCAL_REGISTRATION_INSURANCE";
    return {true, "本地医保统筹仿真支付成功，患者已进入候诊。", data};
}

common::Response BillingService::handleInsuranceCallback(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString transactionId = payload.value("transactionId").toString().trimmed();
    const QString callbackStatus = payload.value("status").toString().trimmed().toUpper();
    const QString callbackSecret = payload.value("callbackSecret").toString();
    if (transactionId.isEmpty() || callbackStatus.isEmpty()) {
        return {false, "医保回调缺少交易号或状态。", {}};
    }
    if (callbackSecret != kInsuranceCallbackSecret) {
        return {false, "医保回调验签失败。", {}};
    }

    auto db = m_database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT t.bill_id, t.amount, b.bill_no, b.status, r.id, r.status, t.status "
                  "FROM insurance_transactions t "
                  "JOIN bills b ON b.id = t.bill_id "
                  "LEFT JOIN registrations r ON r.id = b.registration_id "
                  "WHERE t.transaction_id = :transaction_id LIMIT 1");
    query.bindValue(":transaction_id", transactionId);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到医保交易。", {}};
    }

    const qint64 billId = query.value(0).toLongLong();
    const double amount = query.value(1).toDouble();
    const QString billNo = query.value(2).toString();
    const QString billStatus = query.value(3).toString();
    const qint64 registrationId = query.value(4).toLongLong();
    const QString registrationStatus = query.value(5).toString();
    const QString transactionStatus = query.value(6).toString();
    query.finish();

    if (billStatus == "PAID") {
        db.rollback();
        return {true, "医保回调已处理，该账单已缴费。", paidResponseData(billNo, amount)};
    }
    if (transactionStatus != "PROCESSING") {
        db.rollback();
        QJsonObject data{{"billNo", billNo}, {"transactionId", transactionId}, {"paymentStatus", billStatus}};
        return {true, "医保回调已处理或交易已关闭。", data};
    }

    if (callbackStatus != "SUCCESS") {
        query.prepare("UPDATE insurance_transactions SET status = :status, last_error = :last_error, updated_at = NOW() "
                      "WHERE transaction_id = :transaction_id AND status = 'PROCESSING'");
        query.bindValue(":status", callbackStatus.left(16));
        query.bindValue(":last_error", payload.value("message").toString("医保回调未成功").left(500));
        query.bindValue(":transaction_id", transactionId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        query.prepare("UPDATE bills SET status = 'UNPAID' WHERE id = :id AND status = 'INSURANCE_PROCESSING'");
        query.bindValue(":id", billId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        if (query.numRowsAffected() != 1) {
            db.rollback();
            return {false, "医保失败回调处理失败：账单状态已变化。", {}};
        }
        if (!db.commit()) {
            return {false, db.lastError().text(), {}};
        }
        QJsonObject data{{"billNo", billNo}, {"transactionId", transactionId}, {"paymentStatus", WorkflowRules::pendingPayment()}};
        return {true, "医保未支付成功，账单已回到待支付。", data};
    }

    query.prepare("UPDATE bills SET status = 'PAID', pay_time = NOW(), payment_token_hash = NULL "
                  "WHERE id = :id AND status = 'INSURANCE_PROCESSING'");
    query.bindValue(":id", billId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() != 1) {
        db.rollback();
        return {false, "医保回调处理失败：账单状态已变化。", {}};
    }

    query.prepare("UPDATE registrations SET status = :waiting WHERE id = :id AND status = :pending_payment");
    query.bindValue(":waiting", WorkflowRules::waiting());
    query.bindValue(":id", registrationId);
    query.bindValue(":pending_payment", WorkflowRules::pendingPayment());
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() != 1 && registrationStatus == WorkflowRules::pendingPayment()) {
        db.rollback();
        return {false, "医保回调处理失败：挂号状态已变化。", {}};
    }

    QSqlQuery payment(db);
    payment.prepare("INSERT INTO payments (payment_no, bill_id, amount, pay_method, cashier_id) "
                    "VALUES (:payment_no, :bill_id, :amount, '医保支付', :cashier_id)");
    payment.bindValue(":payment_no", paymentNo("MIPAY"));
    payment.bindValue(":bill_id", billId);
    payment.bindValue(":amount", amount);
    bindCashier(payment, payload);
    if (!payment.exec()) {
        db.rollback();
        return {false, payment.lastError().text(), {}};
    }

    query.prepare("UPDATE insurance_transactions SET status = 'SUCCESS', updated_at = NOW() "
                  "WHERE transaction_id = :transaction_id AND status = 'PROCESSING'");
    query.bindValue(":transaction_id", transactionId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() != 1) {
        db.rollback();
        return {false, "医保回调处理失败：交易状态已变化。", {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data = paidResponseData(billNo, amount);
    data["transactionId"] = transactionId;
    return {true, "医保支付成功，患者已进入候诊。", data};
}

common::Response BillingService::createPaymentQr(const QJsonObject& payload)
{
    const QString traceBillNo = payload.value("billNo").toString(payload.value("账单号").toString()).trimmed();
    qInfo().noquote() << "BillingRequest action=createPaymentQr billNo=" + traceBillNo;
    if (!m_database->ensureOpen()) {
        qWarning().noquote() << "BillingResponse action=createPaymentQr billNo=" + traceBillNo << "success=false error=" + m_database->lastError();
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString billNo = traceBillNo;
    if (billNo.isEmpty()) {
        qWarning().noquote() << "BillingResponse action=createPaymentQr success=false error=missing_bill_no";
        return {false, "缺少账单号，无法生成支付二维码。", {}};
    }

    QJsonObject bill;
    QString error;
    if (!assertBillVisibleToSession(m_database, billNo, payload, &bill, &error)) {
        qWarning().noquote() << "BillingResponse action=createPaymentQr billNo=" + billNo << "success=false error=" + error;
        return {false, error, {}};
    }

    const QString status = bill.value("paymentStatus").toString();
    if (!isPendingBillStatus(status) && status != "PAID") {
        qWarning().noquote() << "BillingResponse action=createPaymentQr billNo=" + billNo << "success=false status=" + status;
        return {false, QString("当前账单状态为 %1，不能生成支付二维码。").arg(status), bill};
    }

    const QString qrPayload = signedQrPayload(billNo,
                                              bill.value("billId").toString().toLongLong(),
                                              payload.value("__operatorUserId").toVariant().toString());
    bill["qrPayload"] = qrPayload;
    bill["qrImageBase64"] = qrSvgBase64(qrPayload);
    bill["qrMimeType"] = "image/svg+xml";
    bill["expiresInSeconds"] = 120;
    qInfo().noquote() << "BillingResponse action=createPaymentQr billNo=" + billNo << "success=true";
    return {true, "支付二维码已生成，请扫码完成自费支付。", bill};
}

common::Response BillingService::checkPayStatus(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString billNo = payload.value("billNo").toString(payload.value("账单号").toString()).trimmed();
    if (billNo.isEmpty()) {
        return {false, "缺少账单号，无法查询支付状态。", {}};
    }

    QJsonObject bill;
    QString error;
    if (!assertBillVisibleToSession(m_database, billNo, payload, &bill, &error)) {
        return {false, error, {}};
    }

    const QString status = bill.value("paymentStatus").toString();
    if (status == "PAID") {
        return {true, "支付成功。", bill};
    }
    if (status == kInsuranceProcessingStatus) {
        return {true, "医保支付处理中。", bill};
    }
    return {true, "等待支付。", bill};
}

common::Response BillingService::requestRefundBill(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString billNo = payload.value("账单号").toString(payload.value("billNo").toString()).trimmed();
    const QString reason = payload.value("退费原因").toString(payload.value("reason").toString()).trimmed();
    if (billNo.isEmpty()) {
        return {false, "请选择要退费的账单。", {}};
    }
    if (reason.isEmpty()) {
        return {false, "退费原因不能为空。", {}};
    }

    auto db = m_database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const qint64 operatorUserId = operatorUserIdFromPayload(payload);

    QSqlQuery query(db);
    query.prepare("SELECT id, total_amount, status FROM bills WHERE bill_no = :bill_no LIMIT 1 FOR UPDATE");
    query.bindValue(":bill_no", billNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (!query.next()) {
        db.rollback();
        return {false, "未找到要退费的账单。", {}};
    }
    const qint64 billId = query.value(0).toLongLong();
    const double amount = query.value(1).toDouble();
    const QString status = query.value(2).toString();
    query.finish();

    if (status != "PAID") {
        db.rollback();
        return {false, "只有已缴费账单可以申请退费。", {}};
    }

    query.prepare("SELECT id FROM refund_requests WHERE bill_id = :bill_id AND status = 'PENDING' LIMIT 1");
    query.bindValue(":bill_id", billId);
    if (!query.exec() || !query.next()) {
        if (query.lastError().isValid()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
    } else {
        db.rollback();
        return {false, "该账单已有待审核退费申请，请勿重复提交。", {}};
    }
    query.finish();

    const QString newRefundNo = refundNo();
    query.prepare("INSERT INTO refund_requests "
                  "(refund_no, bill_id, bill_no, amount, reason, status, requested_by) "
                  "VALUES (:refund_no, :bill_id, :bill_no, :amount, :reason, 'PENDING', :requested_by)");
    query.bindValue(":refund_no", newRefundNo);
    query.bindValue(":bill_id", billId);
    query.bindValue(":bill_no", billNo);
    query.bindValue(":amount", amount);
    query.bindValue(":reason", reason);
    query.bindValue(":requested_by", operatorUserId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    QString error;
    if (!appendBillingOperationLog(db, operatorUserId, "退费申请", QString("账单 %1 申请退费：%2").arg(billNo, reason), &error)) {
        db.rollback();
        return {false, error, {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    QJsonObject data;
    data["refundNo"] = newRefundNo;
    data["billNo"] = billNo;
    data["refundStatus"] = "PENDING";
    return {true, "已提交退费申请，等待审核。", data};
}

common::Response BillingService::reviewRefundBill(const QJsonObject& payload)
{
    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString billNo = payload.value("账单号").toString(payload.value("billNo").toString()).trimmed();
    QString approvedText = payload.value("审核结果").toString(payload.value("approved").toString()).trimmed();
    const QString reviewNote = payload.value("审核意见").toString(payload.value("reviewNote").toString()).trimmed();
    if (billNo.isEmpty()) {
        return {false, "请选择要审核退费的账单。", {}};
    }
    if (approvedText.isEmpty()) {
        approvedText = "通过";
    }
    const bool approved = approvedText == "通过"
        || approvedText.compare("APPROVED", Qt::CaseInsensitive) == 0
        || approvedText.compare("true", Qt::CaseInsensitive) == 0
        || approvedText == "1";

    auto db = m_database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const qint64 reviewerId = operatorUserIdFromPayload(payload);
    QSqlQuery query(db);
    query.prepare("SELECT rr.id, rr.refund_no, rr.bill_id, rr.bill_no, rr.amount, rr.reason, "
                  "b.registration_id, b.status "
                  "FROM refund_requests rr "
                  "JOIN bills b ON b.id = rr.bill_id "
                  "WHERE rr.bill_no = :bill_no AND rr.status = 'PENDING' "
                  "ORDER BY rr.requested_at DESC, rr.id DESC LIMIT 1 FOR UPDATE");
    query.bindValue(":bill_no", billNo);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (!query.next()) {
        db.rollback();
        return {false, "未找到待审核退费申请，请先提交退费申请。", {}};
    }

    const qint64 refundRequestId = query.value(0).toLongLong();
    const QString currentRefundNo = query.value(1).toString();
    const qint64 billId = query.value(2).toLongLong();
    const QString currentBillNo = query.value(3).toString();
    const QString reason = query.value(5).toString();
    const qint64 registrationId = query.value(6).toLongLong();
    const QString billStatus = query.value(7).toString();
    query.finish();

    if (!approved) {
        query.prepare("UPDATE refund_requests SET status = 'REJECTED', reviewed_by = :reviewed_by, "
                      "reviewed_at = NOW(), review_note = :review_note WHERE id = :id AND status = 'PENDING'");
        query.bindValue(":reviewed_by", reviewerId);
        query.bindValue(":review_note", reviewNote);
        query.bindValue(":id", refundRequestId);
        if (!query.exec()) {
            db.rollback();
            return {false, query.lastError().text(), {}};
        }
        QString error;
        if (!appendBillingOperationLog(db, reviewerId, "退费拒绝",
                                       QString("账单 %1 退费拒绝：%2").arg(currentBillNo, reviewNote), &error)) {
            db.rollback();
            return {false, error, {}};
        }
        if (!db.commit()) {
            return {false, db.lastError().text(), {}};
        }
        return {true, "退费申请已拒绝。", QJsonObject{{"refundNo", currentRefundNo}, {"refundStatus", "REJECTED"}}};
    }

    if (billStatus != "PAID") {
        db.rollback();
        return {false, "只有已缴费账单可以审核退费。", {}};
    }

    query.prepare("SELECT COUNT(*) FROM prescriptions WHERE registration_id = :registration_id AND status = 'DISPENSED'");
    query.bindValue(":registration_id", registrationId);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    const int dispensedCount = query.value(0).toInt();
    query.finish();
    if (dispensedCount > 0) {
        QString error;
        if (!appendBillingOperationLog(db, reviewerId, "退费拦截",
                                       QString("账单 %1 存在已发药处方，请先完成药房退药。").arg(currentBillNo), &error)) {
            db.rollback();
            return {false, error, {}};
        }
        if (!db.commit()) {
            return {false, db.lastError().text(), {}};
        }
        return {false, "该账单存在已发药处方，请先完成药房退药后再审核退费。", {}};
    }

    query.prepare("SELECT COUNT(*) FROM prescriptions WHERE registration_id = :registration_id AND status = 'RETURNED'");
    query.bindValue(":registration_id", registrationId);
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    query.finish();

    query.prepare("UPDATE bills SET status = 'REFUNDED' WHERE id = :id AND status = 'PAID'");
    query.bindValue(":id", billId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        return {false, "账单状态已变化，请刷新后重试退费审核。", {}};
    }

    query.prepare("UPDATE refund_requests SET status = 'APPROVED', reviewed_by = :reviewed_by, "
                  "reviewed_at = NOW(), review_note = :review_note WHERE id = :id AND status = 'PENDING'");
    query.bindValue(":reviewed_by", reviewerId);
    query.bindValue(":review_note", reviewNote.isEmpty() ? QStringLiteral("审核通过") : reviewNote);
    query.bindValue(":id", refundRequestId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    if (query.numRowsAffected() == 0) {
        db.rollback();
        return {false, "退费申请状态已变化，请刷新后重试。", {}};
    }

    QString error;
    if (!appendBillingOperationLog(db, reviewerId, "退费审核",
                                   QString("账单 %1 退费审核通过，原因：%2").arg(currentBillNo, reason), &error)) {
        db.rollback();
        return {false, error, {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "退费审核通过，账单状态已更新为已退费。",
            QJsonObject{{"refundNo", currentRefundNo}, {"billNo", currentBillNo}, {"refundStatus", "APPROVED"}}};
}

common::Response BillingService::handle(const common::Request& request)
{
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updateBill(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateBill(request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().cancelBill(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return cancelBill(request.payload);
    }
    if (request.action == "pay" || request.action == "mockPay") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().payBill(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return processSelfPay(request.payload);
    }
    if (request.action == "medicalInsurancePay") {
        if (!m_database->isEnabled()) {
            const QString billNo = request.payload.value("billNo").toString(request.payload.value("账单号").toString());
            QJsonObject data;
            data["billNo"] = billNo;
            data["transactionId"] = "MIDEMO" + QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmsszzz");
            data["paymentStatus"] = "PAID";
            data["registrationStatus"] = WorkflowRules::waiting();
            data["totalAmount"] = request.payload.value("amount").toVariant().toDouble();
            data["simulationMode"] = "LOCAL_REGISTRATION_INSURANCE";
            return {true, "Demo 本地医保统筹仿真支付成功。", data};
        }
        return processMedicalInsurancePay(request.payload);
    }
    if (request.action == "createPaymentQr") {
        if (!m_database->isEnabled()) {
            QJsonObject data;
            data["billNo"] = request.payload.value("billNo").toString(request.payload.value("账单号").toString());
            data["paymentStatus"] = "UNPAID";
            data["qrPayload"] = "BillID=" + data.value("billNo").toString();
            data["qrImageBase64"] = qrSvgBase64(data.value("qrPayload").toString());
            return {true, "Demo 支付二维码已生成。", data};
        }
        return createPaymentQr(request.payload);
    }
    if (request.action == "checkPayStatus") {
        if (!m_database->isEnabled()) {
            QJsonObject data;
            data["billNo"] = request.payload.value("billNo").toString(request.payload.value("账单号").toString());
            data["paymentStatus"] = "PAID";
            data["registrationStatus"] = WorkflowRules::waiting();
            return {true, "Demo 支付成功。", data};
        }
        return checkPayStatus(request.payload);
    }
    if (request.action == "insuranceCallback") {
        if (!m_database->isEnabled()) {
            return {true, "Demo 模式已接收医保回调。", QJsonObject{{"transactionId", request.payload.value("transactionId").toString()}}};
        }
        return handleInsuranceCallback(request.payload);
    }
    if (request.action == "refund") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().refundBill(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return requestRefundBill(request.payload);
    }
    if (request.action == "requestRefund") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().refundBill(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return requestRefundBill(request.payload);
    }
    if (request.action == "reviewRefund") {
        if (!m_database->isEnabled()) {
            QJsonObject data;
            data["refundStatus"] = "APPROVED";
            return {true, "Demo 退费审核通过。", data};
        }
        return reviewRefundBill(request.payload);
    }
    if (request.action != "list") {
        return {false, "Unsupported billing action", {}};
    }

    return SqlJson::selectRows(m_database,
        "SELECT b.bill_no AS '账单号', p.name AS '患者', b.registration_fee AS '挂号费', "
        "b.drug_fee AS '药品费', b.other_fee AS '其他费用', b.total_amount AS '合计', "
        "CASE b.status WHEN 'UNPAID' THEN '待缴费' WHEN 'PENDING' THEN '待缴费' "
        "WHEN 'PENDING_PAYMENT' THEN '待缴费' WHEN 'INSURANCE_PROCESSING' THEN '医保处理中' "
        "WHEN 'PAID' THEN '已缴费' WHEN 'REFUNDED' THEN '已退费' "
        "WHEN 'CANCELLED' THEN '已取消' ELSE b.status END AS '状态', "
        "COALESCE(CASE rr.status WHEN 'PENDING' THEN '待审核' WHEN 'APPROVED' THEN '已通过' "
        "WHEN 'REJECTED' THEN '已拒绝' ELSE rr.status END, '') AS '退费状态', "
        "COALESCE(rr.refund_no, '') AS '退费单号', COALESCE(rr.reason, '') AS '退费原因', "
        "COALESCE(rr.review_note, '') AS '审核意见', "
        "COALESCE(pay.pay_method, '') AS '支付方式', "
        "COALESCE(pay.payment_no, '') AS '发票号', COALESCE(pay.pay_time, '') AS '支付时间', "
        "b.created_at AS '创建时间' "
        "FROM bills b "
        "JOIN patients p ON p.id = b.patient_id "
        "LEFT JOIN refund_requests rr ON rr.id = ("
        "SELECT rr2.id FROM refund_requests rr2 WHERE rr2.bill_id = b.id ORDER BY rr2.requested_at DESC, rr2.id DESC LIMIT 1"
        ") "
        "LEFT JOIN payments pay ON pay.id = ("
        "SELECT p2.id FROM payments p2 WHERE p2.bill_id = b.id ORDER BY p2.pay_time DESC, p2.id DESC LIMIT 1"
        ") "
        "ORDER BY b.created_at DESC LIMIT 100",
        {}, "bills");
}

} // namespace hospital::server
