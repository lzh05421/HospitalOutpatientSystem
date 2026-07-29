#pragma once

#include "common/Protocol.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <stdexcept>

class QTimer;

namespace hospital { namespace client {

class ApiClient;

class TimeoutException final : public std::runtime_error
{
public:
    explicit TimeoutException(const QString& message);
};

struct PaymentError
{
    QString action;
    QString billNo;
    QString userMessage;
    QString technicalMessage;
    QString errorCode;
};

class PaymentNetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit PaymentNetworkManager(ApiClient* apiClient, QObject* parent = nullptr);

    void createPaymentQr(const QString& billNo, double amount, const QString& notifyUrl);
    void startMedicalInsurancePay(const QString& billNo, double amount, const QString& paymentToken);
    void payBill(const QString& billNo, double amount, const QString& paymentToken, const QString& payMethod);
    void checkPayStatus(const QString& billNo, bool forceServer = false);
    bool hasPendingRequest() const;
    void cancelPendingRequest();

signals:
    void qrReady(const QJsonObject& data);
    void medicalInsuranceReady(const QJsonObject& data);
    void paymentStatusReady(const QJsonObject& data);
    void paymentConfirmationDelayed(const QString& billNo);
    void medicalInsuranceSlow();
    void requestFailed(const PaymentError& error);

private slots:
    void onResponseReceived(const common::Response& response);
    void onSocketError(const QString& message);
    void onRequestTimeout();
    void onMedicalSlowTimeout();

private:
    enum class RequestKind {
        None,
        CreateQr,
        MedicalInsurance,
        SelfPay,
        CheckStatus
    };

    void sendRequest(const common::Request& request, RequestKind kind, int timeoutMs);
    void finishRequest();
    void fail(RequestKind kind,
              const QString& userMessage,
              const QString& technicalMessage,
              const QString& errorCode);
    bool handleLatePaymentConfirmation(const common::Response& response);
    bool validateQrResponse(const QJsonObject& data, PaymentError* error) const;
    bool validateInsuranceResponse(const QJsonObject& data, PaymentError* error) const;
    bool validateStatusResponse(const QJsonObject& data, PaymentError* error) const;
    QString actionForKind(RequestKind kind) const;
    QString friendlyMessageForAction(const QString& action, const QString& message) const;
    void appendPaymentLog(const PaymentError& error) const;

    ApiClient* m_apiClient = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    QTimer* m_medicalSlowTimer = nullptr;
    RequestKind m_pendingKind = RequestKind::None;
    QString m_pendingBillNo;
    QString m_confirmationBillNo;
    double m_pendingAmount = 0.0;
};

}} // namespace hospital::client
