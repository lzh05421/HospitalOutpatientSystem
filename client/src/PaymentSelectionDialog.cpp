#include "client/PaymentSelectionDialog.h"

#include "client/ApiClient.h"
#include "client/PaymentNetworkManager.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

constexpr int kConfirmationPollIntervalMs = 2000;
constexpr int kMaxLoadingConfirmationAttempts = 6;
constexpr qsizetype kMaxMockQrPayloadBytes = 108;

QPushButton* paymentButton(const QString& iconPlaceholder,
                           const QString& title,
                           const QString& description,
                           QWidget* parent)
{
    auto* button = new QPushButton(QString("%1  %2\n%3").arg(iconPlaceholder, title, description), parent);
    button->setMinimumHeight(72);
    button->setStyleSheet("text-align: left; padding: 10px 14px; font-weight: 600;");
    return button;
}

namespace ErrorHandler {

QString userMessage(const PaymentError& error)
{
    if (error.errorCode == "TIMEOUT") {
        return "网络连接超时，请检查网络后重试。";
    }
    return error.userMessage.isEmpty() ? "支付服务暂时不可用，请稍后重试。" : error.userMessage;
}

} // namespace ErrorHandler

} // namespace

PaymentSelectionDialog::PaymentSelectionDialog(ApiClient* apiClient,
                                               const QString& billNo,
                                               double totalAmount,
                                               const QString& paymentToken,
                                               bool registrationInsuranceApproved,
                                               QWidget* parent)
    : QDialog(parent)
    , m_apiClient(apiClient)
    , m_networkManager(new PaymentNetworkManager(apiClient, this))
    , m_billNo(billNo)
    , m_paymentToken(paymentToken)
    , m_registrationInsuranceApproved(registrationInsuranceApproved)
    , m_totalAmount(totalAmount)
{
    setWindowTitle("选择支付方式");
    setFixedSize(520, 660);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kConfirmationPollIntervalMs);
    m_scanStatusManager = new QNetworkAccessManager(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto* titleLabel = new QLabel(QString("账单号：%1    金额：%2 元")
                                      .arg(m_billNo)
                                      .arg(m_totalAmount, 0, 'f', 2),
                                  this);
    titleLabel->setWordWrap(true);
    m_statusLabel = new QLabel("请选择支付方式", this);
    m_statusLabel->setObjectName("paymentStatusLabel");
    m_statusLabel->setWordWrap(true);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);

    m_stack = new QStackedWidget(this);

    auto* choicePage = new QWidget(m_stack);
    auto* choiceLayout = new QVBoxLayout(choicePage);
    choiceLayout->setContentsMargins(0, 0, 0, 0);
    choiceLayout->setSpacing(10);
    m_directButton = paymentButton("[WX/AL]", "自费支付", "微信/支付宝扫一扫，直接支付", choicePage);
    m_insuranceButton = paymentButton("[MI]", "医保统筹",
                                      m_registrationInsuranceApproved
                                          ? "挂号医保资格已通过，扫码支付报销后个人自付金额"
                                          : "需先在挂号页选择医保统筹并通过资格校验",
                                      choicePage);
    m_insuranceButton->setEnabled(m_registrationInsuranceApproved);
    m_retryButton = new QPushButton("重试", choicePage);
    m_retryButton->setVisible(false);
    choiceLayout->addWidget(m_directButton);
    choiceLayout->addWidget(m_insuranceButton);
    choiceLayout->addWidget(m_retryButton);
    choiceLayout->addStretch();

    auto* qrPage = new QWidget(m_stack);
    auto* qrLayout = new QVBoxLayout(qrPage);
    qrLayout->setContentsMargins(0, 0, 0, 0);
    qrLayout->setSpacing(14);
    m_qrLabel = new QLabel(qrPage);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setFixedSize(340, 340);
    m_qrLabel->setStyleSheet("background: white; border: 1px solid #E5E7EB;");
    m_mockConfirmButton = new QPushButton("等待手机扫码自动确认", qrPage);
    m_mockConfirmButton->setObjectName("primaryButton");
    m_mockConfirmButton->setFixedHeight(64);
    m_mockConfirmButton->setVisible(false);
    qrLayout->addStretch();
    qrLayout->addWidget(m_qrLabel, 0, Qt::AlignHCenter);
    qrLayout->addWidget(m_mockConfirmButton);
    qrLayout->addStretch();

    m_stack->addWidget(choicePage);
    m_stack->addWidget(qrPage);

    root->addWidget(titleLabel);
    root->addWidget(m_statusLabel);
    root->addWidget(m_progressBar);
    root->addWidget(m_stack, 1);

    connect(m_directButton, &QPushButton::clicked, this, &PaymentSelectionDialog::onDirectPaymentClicked);
    connect(m_insuranceButton, &QPushButton::clicked, this, &PaymentSelectionDialog::onMedicalInsurancePaymentClicked);
    connect(m_retryButton, &QPushButton::clicked, this, &PaymentSelectionDialog::retryLastAction);
    connect(m_mockConfirmButton, &QPushButton::clicked, this, &PaymentSelectionDialog::simulateLocalScanPayment);
    connect(m_pollTimer, &QTimer::timeout, this, &PaymentSelectionDialog::pollPaymentStatus);
    connect(m_networkManager, &PaymentNetworkManager::qrReady, this, [this](const QJsonObject& data) {
        showQrImage(data.value("qrImageBase64").toString(), data.value("qrPayload").toString());
        m_mockConfirmButton->setText("手机无法打开时，点击本机模拟扫码支付");
        startConfirmationPolling("请用手机微信或支付宝扫一扫；若手机无法加载网页，可点击下方本机模拟扫码支付。");
    });
    connect(m_networkManager, &PaymentNetworkManager::paymentStatusReady, this, [this](const QJsonObject& data) {
        const QString status = data.value("paymentStatus").toString();
        if (status == "PAID") {
            finishAsPaid("支付成功。");
        } else {
            if (m_state == PaymentUiState::ConfirmingPayment
                || m_state == PaymentUiState::ConfirmationSlow
                || m_lastAction == "direct") {
                startConfirmationPolling(status == "INSURANCE_PROCESSING"
                                             ? "医保支付处理中，系统正在自动刷新状态..."
                                             : "正在自动确认支付结果，请稍候...");
            } else {
                m_statusLabel->setText(status == "INSURANCE_PROCESSING" ? "医保支付处理中..." : "等待支付完成...");
            }
        }
    });
    connect(m_networkManager, &PaymentNetworkManager::paymentConfirmationDelayed, this, [this](const QString& billNo) {
        Q_UNUSED(billNo);
        startConfirmationPolling("服务端确认较慢，系统正在自动查询支付结果...");
    });
    connect(m_networkManager, &PaymentNetworkManager::requestFailed, this, &PaymentSelectionDialog::failCurrentRequest);

    if (m_registrationInsuranceApproved) {
        m_statusLabel->setText("医保统筹资格已通过，将按报销后个人自付金额生成扫码二维码。");
        QTimer::singleShot(200, this, &PaymentSelectionDialog::onMedicalInsurancePaymentClicked);
    }
}

void PaymentSelectionDialog::onDirectPaymentClicked()
{
    m_lastAction = "direct";
    m_confirmationAttempts = 0;
    if (m_paymentToken.trimmed().isEmpty()) {
        enterState(PaymentUiState::Error, "当前账单缺少扫码支付凭证，不能用手机扫码确认。请从新增挂号后的待支付弹窗重新发起直接支付。");
        return;
    }
    setButtonsLoading(m_directButton);
    enterState(PaymentUiState::LoadingQr, "正在建立安全支付会话...");
    m_networkManager->createPaymentQr(m_billNo, m_totalAmount, serverPaymentCallbackUrl());
}

void PaymentSelectionDialog::onMedicalInsurancePaymentClicked()
{
    if (!m_registrationInsuranceApproved) {
        QMessageBox::warning(this, "医保统筹不可用",
                             "本次挂号未通过医保统筹资格校验，无法使用医保统筹支付。请返回挂号页选择“医保统筹”并完成校验，或使用自费支付。");
        return;
    }
    m_lastAction = "insurance";
    m_confirmationAttempts = 0;
    if (m_paymentToken.trimmed().isEmpty()) {
        enterState(PaymentUiState::Error, "当前账单缺少扫码支付凭证，不能用手机扫码确认。请从新增挂号后的待支付弹窗重新发起支付。");
        return;
    }
    m_lastAction = "direct";
    setButtonsLoading(m_insuranceButton);
    enterState(PaymentUiState::LoadingQr, "正在生成医保统筹个人自付金额支付二维码...");
    m_networkManager->createPaymentQr(m_billNo, m_totalAmount, serverPaymentCallbackUrl());
}

void PaymentSelectionDialog::pollPaymentStatus()
{
    if (m_lastAction == "direct" && !m_paymentToken.trimmed().isEmpty()) {
        pollScanPaymentStatus();
        return;
    }
    if (!m_networkManager->hasPendingRequest()) {
        ++m_confirmationAttempts;
        m_networkManager->checkPayStatus(m_billNo, true);
    }
}

void PaymentSelectionDialog::pollScanPaymentStatus()
{
    if (m_scanStatusReply) {
        return;
    }

    ++m_confirmationAttempts;
    QNetworkRequest request{QUrl(scanPaymentStatusUrl())};
    m_scanStatusReply = m_scanStatusManager->get(request);
    QNetworkReply* currentReply = m_scanStatusReply;
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply]() {
        QNetworkReply* reply = currentReply;
        if (m_scanStatusReply == reply) {
            m_scanStatusReply = nullptr;
        }
        const QByteArray body = reply->readAll();
        const bool networkOk = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();

        if (!networkOk) {
            if (m_confirmationAttempts >= kMaxLoadingConfirmationAttempts) {
                enterState(PaymentUiState::ConfirmationSlow, "正在等待手机扫码结果，请确认手机和电脑在同一网络...");
            }
            return;
        }

        const QJsonObject data = QJsonDocument::fromJson(body).object();
        if (data.value("paymentStatus").toString() == "PAID") {
            finishAsPaid("支付成功。");
            return;
        }
        if (m_confirmationAttempts >= kMaxLoadingConfirmationAttempts) {
            enterState(PaymentUiState::ConfirmationSlow, "尚未收到手机扫码成功结果，系统会继续自动刷新...");
        }
    });
}

void PaymentSelectionDialog::onMedicalPaymentSuccessUrl()
{
    m_pollTimer->stop();
    emit paymentPending(m_billNo);
    accept();
}

void PaymentSelectionDialog::enterState(PaymentUiState state, const QString& message)
{
    m_state = state;
    if (!message.isEmpty()) {
        m_statusLabel->setText(message);
    }
    const bool loading = state == PaymentUiState::LoadingQr
        || state == PaymentUiState::ConfirmingPayment
        || state == PaymentUiState::ConfirmationSlow;
    m_progressBar->setVisible(loading);
    m_retryButton->setVisible(state == PaymentUiState::Error);
    if (m_mockConfirmButton) {
        const bool canSimulateScan = m_lastAction == "direct"
            && !m_paymentToken.trimmed().isEmpty()
            && (state == PaymentUiState::ShowingQr
                || state == PaymentUiState::ConfirmingPayment
                || state == PaymentUiState::ConfirmationSlow);
        m_mockConfirmButton->setVisible(canSimulateScan);
    }
    if (state == PaymentUiState::Idle || state == PaymentUiState::Error) {
        m_stack->setCurrentIndex(0);
    } else if (state == PaymentUiState::ShowingQr
               || state == PaymentUiState::ConfirmingPayment
               || state == PaymentUiState::ConfirmationSlow) {
        m_stack->setCurrentIndex(1);
    }
}

void PaymentSelectionDialog::setButtonsLoading(QPushButton* activeButton)
{
    m_directButton->setEnabled(false);
    m_insuranceButton->setEnabled(false);
    if (m_mockConfirmButton) {
        m_mockConfirmButton->setEnabled(activeButton != m_mockConfirmButton);
    }
    activeButton->setText("加载中...");
    m_statusLabel->setText("正在建立安全支付会话...");
}

void PaymentSelectionDialog::restoreButtons()
{
    if (m_scanStatusReply) {
        disconnect(m_scanStatusReply, nullptr, this, nullptr);
        m_scanStatusReply->abort();
        m_scanStatusReply->deleteLater();
        m_scanStatusReply = nullptr;
    }
    m_directButton->setText("[WX/AL]  自费支付\n微信/支付宝扫一扫，直接支付");
    m_insuranceButton->setText(m_registrationInsuranceApproved
                                   ? "[MI]  医保统筹\n挂号医保资格已通过，扫码支付报销后个人自付金额"
                                   : "[MI]  医保统筹\n需先在挂号页选择医保统筹并通过资格校验");
    if (m_mockConfirmButton) {
        m_mockConfirmButton->setText("手机无法打开时，点击本机模拟扫码支付");
        m_mockConfirmButton->setEnabled(true);
    }
    m_directButton->setEnabled(true);
    m_insuranceButton->setEnabled(m_registrationInsuranceApproved);
    m_confirmationAttempts = 0;
    enterState(PaymentUiState::Idle, "请选择支付方式");
}

void PaymentSelectionDialog::retryLastAction()
{
    if (m_lastAction == "insurance") {
        onMedicalInsurancePaymentClicked();
        return;
    }
    onDirectPaymentClicked();
}

void PaymentSelectionDialog::startConfirmationPolling(const QString& message)
{
    if (m_state != PaymentUiState::ConfirmingPayment && m_state != PaymentUiState::ConfirmationSlow) {
        m_confirmationAttempts = 0;
    }
    const QString text = message.isEmpty()
        ? QString("正在自动确认支付结果，请稍候...")
        : message;
    if (m_confirmationAttempts >= kMaxLoadingConfirmationAttempts) {
        enterState(PaymentUiState::ConfirmationSlow, "支付确认比平时慢，系统会继续后台自动刷新...");
    } else {
        enterState(PaymentUiState::ConfirmingPayment, text);
    }
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
    if (!m_networkManager->hasPendingRequest()) {
        QTimer::singleShot(300, this, &PaymentSelectionDialog::pollPaymentStatus);
    }
}

void PaymentSelectionDialog::showQrImage(const QString& base64Image, const QString& payloadText)
{
    const QByteArray bytes = QByteArray::fromBase64(base64Image.toLatin1());
    QPixmap pixmap;
    if (pixmap.loadFromData(bytes)) {
        m_qrLabel->clear();
        m_qrLabel->setWordWrap(false);
        m_qrLabel->setPixmap(pixmap.scaled(320, 320, Qt::KeepAspectRatio, Qt::FastTransformation));
    } else {
        m_qrLabel->setPixmap(QPixmap());
        m_qrLabel->setText(payloadText);
        m_qrLabel->setWordWrap(true);
    }
}

void PaymentSelectionDialog::simulateLocalScanPayment()
{
    if (m_paymentToken.trimmed().isEmpty()) {
        enterState(PaymentUiState::Error, "当前账单缺少支付凭证，不能进行本机模拟扫码支付。");
        return;
    }
    m_lastAction = "direct";
    setButtonsLoading(m_mockConfirmButton);
    enterState(PaymentUiState::ConfirmingPayment, "正在执行本机模拟扫码支付...");
    m_networkManager->payBill(m_billNo, m_totalAmount, m_paymentToken, "微信/支付宝扫码（本机仿真）");
}

void PaymentSelectionDialog::finishAsPaid(const QString& message)
{
    m_pollTimer->stop();
    if (m_scanStatusReply) {
        disconnect(m_scanStatusReply, nullptr, this, nullptr);
        m_scanStatusReply->abort();
        m_scanStatusReply->deleteLater();
        m_scanStatusReply = nullptr;
    }
    m_networkManager->cancelPendingRequest();
    QMessageBox::information(this, "支付成功", message.isEmpty() ? "支付成功，订单状态已更新。" : message);
    emit paymentCompleted(m_billNo);
    accept();
}

void PaymentSelectionDialog::failCurrentRequest(const PaymentError& error)
{
    const bool canKeepConfirming = m_lastAction == "direct"
        && error.errorCode == "TIMEOUT"
        && (error.action == "pay" || error.action == "checkPayStatus");
    if (canKeepConfirming) {
        startConfirmationPolling("支付结果仍在后台确认，系统会继续自动刷新...");
        return;
    }

    m_pollTimer->stop();
    if (m_scanStatusReply) {
        disconnect(m_scanStatusReply, nullptr, this, nullptr);
        m_scanStatusReply->abort();
        m_scanStatusReply->deleteLater();
        m_scanStatusReply = nullptr;
    }
    restoreButtons();
    enterState(PaymentUiState::Error, ErrorHandler::userMessage(error));
}

QString PaymentSelectionDialog::serverPaymentCallbackUrl() const
{
    return paymentServerUrl("/p", true);
}

QString PaymentSelectionDialog::scanPaymentStatusUrl() const
{
    return paymentServerUrl("/p-status", false);
}

QString PaymentSelectionDialog::paymentServerUrl(const QString& path, bool logUrl) const
{
    const QString configuredHost = m_apiClient ? m_apiClient->serverHost().trimmed() : QString();
    QString host = configuredHost;
    if (host.isEmpty()
        || host == "127.0.0.1"
        || host == "localhost"
        || host == "0.0.0.0"
        || host == "::1") {
        host = localCallbackHost();
    }

    QUrl url;
    url.setScheme("http");
    url.setHost(host);
    url.setPort(m_apiClient ? m_apiClient->serverPort() : 8899);
    url.setPath(path);
    QUrlQuery query;
    query.addQueryItem("b", m_billNo);
    query.addQueryItem("t", m_paymentToken.trimmed());
    url.setQuery(query);
    if (logUrl && url.toString(QUrl::FullyEncoded).toUtf8().size() > kMaxMockQrPayloadBytes) {
        qWarning().noquote()
            << "MockPaymentCallbackUrl exceeds QR payload limit"
            << url.toString(QUrl::FullyEncoded).toUtf8().size()
            << "limit=" << kMaxMockQrPayloadBytes;
    }
    if (logUrl) {
        qInfo().noquote()
        << "MockPaymentCallbackUrl"
        << url.toString(QUrl::FullyEncoded);
    }
    return url.toString(QUrl::FullyEncoded);
}

QString PaymentSelectionDialog::localCallbackHost() const
{
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback()) {
            continue;
        }
        const QString text = address.toString();
        if (text.startsWith("192.168.") || text.startsWith("10.") || text.startsWith("172.")) {
            return text;
        }
    }
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return "127.0.0.1";
}

} // namespace hospital::client
