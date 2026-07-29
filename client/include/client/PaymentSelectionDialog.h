#pragma once

#include "client/PaymentNetworkManager.h"

#include <QDialog>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace hospital { namespace client {

class ApiClient;

class PaymentSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    PaymentSelectionDialog(ApiClient* apiClient,
                           const QString& billNo,
                           double totalAmount,
                           const QString& paymentToken = {},
                           bool registrationInsuranceApproved = false,
                           QWidget* parent = nullptr);

signals:
    void paymentCompleted(const QString& billNo);
    void paymentPending(const QString& billNo);

private slots:
    void onDirectPaymentClicked();
    void onMedicalInsurancePaymentClicked();
    void pollPaymentStatus();
    void onMedicalPaymentSuccessUrl();
    void pollScanPaymentStatus();

private:
    enum class PaymentUiState {
        Idle,
        LoadingQr,
        ShowingQr,
        ConfirmingPayment,
        ConfirmationSlow,
        Error
    };

    void enterState(PaymentUiState state, const QString& message = {});
    void setButtonsLoading(QPushButton* activeButton);
    void restoreButtons();
    void retryLastAction();
    void startConfirmationPolling(const QString& message = {});
    void showQrImage(const QString& base64Image, const QString& payloadText);
    void simulateLocalScanPayment();
    void finishAsPaid(const QString& message);
    void failCurrentRequest(const PaymentError& error);
    QString serverPaymentCallbackUrl() const;
    QString scanPaymentStatusUrl() const;
    QString paymentServerUrl(const QString& path, bool logUrl) const;
    QString localCallbackHost() const;

    ApiClient* m_apiClient = nullptr;
    PaymentNetworkManager* m_networkManager = nullptr;
    QString m_billNo;
    QString m_paymentToken;
    bool m_registrationInsuranceApproved = false;
    double m_totalAmount = 0.0;
    QPushButton* m_directButton = nullptr;
    QPushButton* m_insuranceButton = nullptr;
    QPushButton* m_retryButton = nullptr;
    QPushButton* m_mockConfirmButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_qrLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QTimer* m_pollTimer = nullptr;
    QNetworkAccessManager* m_scanStatusManager = nullptr;
    QNetworkReply* m_scanStatusReply = nullptr;
    QString m_mockCallbackToken;
    QString m_lastAction;
    int m_confirmationAttempts = 0;
    PaymentUiState m_state = PaymentUiState::Idle;
};

}} // namespace hospital::client
