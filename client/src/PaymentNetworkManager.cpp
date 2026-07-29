#include "client/PaymentNetworkManager.h"

#include "client/ApiClient.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QVector>

namespace hospital::client {
namespace {

constexpr int kPaymentRequestTimeoutMs = 8000;
constexpr int kSelfPayRequestTimeoutMs = 10000;
constexpr int kMedicalRequestTimeoutMs = 8000;
constexpr int kMedicalSlowHintMs = 3000;
constexpr bool kMockPaymentOnTimeout = true;
constexpr bool kForceMockPaymentFlow = true;
constexpr int kMockQrVersion = 5;
constexpr int kMockQrSize = 17 + kMockQrVersion * 4;
constexpr int kMockQrDataCodewords = 108;
constexpr int kMockQrEccCodewords = 26;
constexpr int kMockQrQuietZone = 4;
constexpr int kMockQrScale = 8;

double amountFromPayload(const QJsonObject& payload)
{
    const QJsonValue value = payload.contains("amount")
        ? payload.value("amount")
        : payload.contains("Amount")
            ? payload.value("Amount")
            : payload.value("合计");
    return value.toVariant().toDouble();
}

int gfMultiply(int x, int y)
{
    int result = 0;
    while (y > 0) {
        if ((y & 1) != 0) {
            result ^= x;
        }
        x <<= 1;
        if ((x & 0x100) != 0) {
            x ^= 0x11D;
        }
        y >>= 1;
    }
    return result;
}

int gfPow(int x, int power)
{
    int result = 1;
    for (int i = 0; i < power; ++i) {
        result = gfMultiply(result, x);
    }
    return result;
}

QVector<int> reedSolomonGenerator(int degree)
{
    QVector<int> result(degree, 0);
    result[degree - 1] = 1;
    int root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = gfMultiply(result[j], root);
            if (j + 1 < degree) {
                result[j] ^= result[j + 1];
            }
        }
        root = gfMultiply(root, 2);
    }
    return result;
}

QVector<int> reedSolomonRemainder(const QVector<int>& data, const QVector<int>& generator)
{
    QVector<int> result(generator.size(), 0);
    for (int value : data) {
        const int factor = value ^ result.first();
        result.removeFirst();
        result.append(0);
        for (int i = 0; i < result.size(); ++i) {
            result[i] ^= gfMultiply(generator[i], factor);
        }
    }
    return result;
}

void appendBits(QVector<bool>* bits, int value, int count)
{
    for (int i = count - 1; i >= 0; --i) {
        bits->append(((value >> i) & 1) != 0);
    }
}

QVector<int> makeQrCodewords(const QString& payload)
{
    const QByteArray bytes = payload.toUtf8();
    QVector<bool> bits;
    appendBits(&bits, 0x4, 4);
    appendBits(&bits, bytes.size(), 8);
    for (const char byte : bytes) {
        appendBits(&bits, static_cast<unsigned char>(byte), 8);
    }

    const int capacityBits = kMockQrDataCodewords * 8;
    const int terminatorBits = qMin(4, capacityBits - bits.size());
    appendBits(&bits, 0, terminatorBits);
    while ((bits.size() % 8) != 0) {
        bits.append(false);
    }

    QVector<int> data;
    for (int i = 0; i < bits.size(); i += 8) {
        int value = 0;
        for (int j = 0; j < 8; ++j) {
            value = (value << 1) | (bits[i + j] ? 1 : 0);
        }
        data.append(value);
    }

    for (int pad = 0xEC; data.size() < kMockQrDataCodewords; pad ^= 0xEC ^ 0x11) {
        data.append(pad);
    }

    const QVector<int> generator = reedSolomonGenerator(kMockQrEccCodewords);
    const QVector<int> ecc = reedSolomonRemainder(data, generator);
    QVector<int> all = data;
    all += ecc;
    return all;
}

void setModule(QVector<QVector<bool>>* modules,
               QVector<QVector<bool>>* functionModules,
               int x,
               int y,
               bool dark,
               bool isFunction = true)
{
    if (x < 0 || y < 0 || x >= kMockQrSize || y >= kMockQrSize) {
        return;
    }
    (*modules)[y][x] = dark;
    if (isFunction) {
        (*functionModules)[y][x] = true;
    }
}

void drawFinder(QVector<QVector<bool>>* modules, QVector<QVector<bool>>* functionModules, int x, int y)
{
    for (int dy = -1; dy <= 7; ++dy) {
        for (int dx = -1; dx <= 7; ++dx) {
            const int xx = x + dx;
            const int yy = y + dy;
            const bool inCore = dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6;
            const bool dark = inCore && (dx == 0 || dx == 6 || dy == 0 || dy == 6 || (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4));
            setModule(modules, functionModules, xx, yy, dark);
        }
    }
}

void drawAlignment(QVector<QVector<bool>>* modules, QVector<QVector<bool>>* functionModules, int centerX, int centerY)
{
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const bool dark = qMax(qAbs(dx), qAbs(dy)) != 1;
            setModule(modules, functionModules, centerX + dx, centerY + dy, dark);
        }
    }
}

int formatBits()
{
    int data = 0x08; // Error correction L, mask pattern 0.
    int bits = data << 10;
    for (int i = 14; i >= 10; --i) {
        if (((bits >> i) & 1) != 0) {
            bits ^= 0x537 << (i - 10);
        }
    }
    return ((data << 10) | bits) ^ 0x5412;
}

void drawFormatBits(QVector<QVector<bool>>* modules, QVector<QVector<bool>>* functionModules)
{
    const int bits = formatBits();
    auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; ++i) {
        setModule(modules, functionModules, 8, i, bit(i));
    }
    setModule(modules, functionModules, 8, 7, bit(6));
    setModule(modules, functionModules, 8, 8, bit(7));
    setModule(modules, functionModules, 7, 8, bit(8));
    for (int i = 9; i < 15; ++i) {
        setModule(modules, functionModules, 14 - i, 8, bit(i));
    }
    for (int i = 0; i < 8; ++i) {
        setModule(modules, functionModules, kMockQrSize - 1 - i, 8, bit(i));
    }
    for (int i = 8; i < 15; ++i) {
        setModule(modules, functionModules, 8, kMockQrSize - 15 + i, bit(i));
    }
    setModule(modules, functionModules, 8, kMockQrSize - 8, true);
}

QVector<QVector<bool>> makeQrModules(const QString& payload)
{
    QVector<QVector<bool>> modules(kMockQrSize, QVector<bool>(kMockQrSize, false));
    QVector<QVector<bool>> functionModules(kMockQrSize, QVector<bool>(kMockQrSize, false));

    drawFinder(&modules, &functionModules, 0, 0);
    drawFinder(&modules, &functionModules, kMockQrSize - 7, 0);
    drawFinder(&modules, &functionModules, 0, kMockQrSize - 7);
    drawAlignment(&modules, &functionModules, 30, 30);
    for (int i = 8; i < kMockQrSize - 8; ++i) {
        setModule(&modules, &functionModules, 6, i, (i % 2) == 0);
        setModule(&modules, &functionModules, i, 6, (i % 2) == 0);
    }
    drawFormatBits(&modules, &functionModules);

    const QVector<int> codewords = makeQrCodewords(payload);
    int bitIndex = 0;
    int direction = -1;
    int y = kMockQrSize - 1;
    for (int right = kMockQrSize - 1; right >= 1; right -= 2) {
        if (right == 6) {
            --right;
        }
        while (true) {
            for (int dx = 0; dx < 2; ++dx) {
                const int x = right - dx;
                if (!functionModules[y][x]) {
                    bool dark = false;
                    if (bitIndex < codewords.size() * 8) {
                        dark = ((codewords[bitIndex / 8] >> (7 - (bitIndex % 8))) & 1) != 0;
                    }
                    if (((x + y) % 2) == 0) {
                        dark = !dark;
                    }
                    setModule(&modules, &functionModules, x, y, dark, false);
                    ++bitIndex;
                }
            }
            y += direction;
            if (y < 0 || y >= kMockQrSize) {
                y -= direction;
                direction = -direction;
                break;
            }
        }
    }

    return modules;
}

QString realQrImageBase64(const QString& payload)
{
    const QVector<QVector<bool>> modules = makeQrModules(payload);
    const int imageModules = kMockQrSize + kMockQrQuietZone * 2;
    const int imageSize = imageModules * kMockQrScale;
    QImage image(imageSize, imageSize, QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < kMockQrSize; ++y) {
        for (int x = 0; x < kMockQrSize; ++x) {
            if (!modules[y][x]) {
                continue;
            }
            painter.drawRect((x + kMockQrQuietZone) * kMockQrScale,
                             (y + kMockQrQuietZone) * kMockQrScale,
                             kMockQrScale,
                             kMockQrScale);
        }
    }
    painter.end();

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(pngBytes.toBase64());
}

QJsonObject mockQrResponse(const QString& billNo, double amount, const QString& action, const QString& payUrl)
{
    const QString qrPayload = payUrl.trimmed().isEmpty()
        ? QStringLiteral("hospital://payment/notify")
        : payUrl.trimmed();
    QJsonObject data;
    data["module"] = "billing";
    data["action"] = action;
    data["billNo"] = billNo;
    data["amount"] = amount;
    data["Amount"] = amount;
    data["payUrl"] = qrPayload;
    data["qrPayload"] = qrPayload;
    data["qrImageBase64"] = realQrImageBase64(qrPayload);
    data["qrMimeType"] = "image/png";
    data["paymentStatus"] = "UNPAID";
    data["mock"] = true;
    return data;
}

QJsonObject mockPaidStatusResponse(const QString& billNo, double amount)
{
    QJsonObject data;
    data["module"] = "billing";
    data["action"] = "checkPayStatus";
    data["billNo"] = billNo;
    data["amount"] = amount;
    data["paymentStatus"] = "UNPAID";
    data["registrationStatus"] = "PENDING_PAYMENT";
    data["mock"] = true;
    return data;
}

QString logFilePath()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dirPath = baseDir.isEmpty() ? QDir::tempPath() + "/HospitalOutpatientSystem" : baseDir;
    QDir().mkpath(dirPath + "/logs");
    return dirPath + "/logs/payment.log";
}

} // namespace

TimeoutException::TimeoutException(const QString& message)
    : std::runtime_error(message.toStdString())
{
}

PaymentNetworkManager::PaymentNetworkManager(ApiClient* apiClient, QObject* parent)
    : QObject(parent)
    , m_apiClient(apiClient)
    , m_timeoutTimer(new QTimer(this))
    , m_medicalSlowTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_medicalSlowTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &PaymentNetworkManager::onRequestTimeout);
    connect(m_medicalSlowTimer, &QTimer::timeout, this, &PaymentNetworkManager::onMedicalSlowTimeout);
    connect(m_apiClient, &ApiClient::responseReceived, this, &PaymentNetworkManager::onResponseReceived);
    connect(m_apiClient, &ApiClient::errorOccurred, this, &PaymentNetworkManager::onSocketError);
}

void PaymentNetworkManager::createPaymentQr(const QString& billNo, double amount, const QString& notifyUrl)
{
    if (kForceMockPaymentFlow) {
        qWarning().noquote() << "Warning: Payment MOCK mode enabled, generating QR locally.";
        const QJsonObject data = mockQrResponse(billNo, amount, "createPaymentQr", notifyUrl);
        QTimer::singleShot(0, this, [this, data]() {
            emit qrReady(data);
        });
        return;
    }

    common::Request request;
    request.module = "billing";
    request.action = "createPaymentQr";
    request.payload["billNo"] = billNo;
    request.payload["orderId"] = billNo;
    request.payload["OrderID"] = billNo;
    request.payload["amount"] = amount;
    request.payload["Amount"] = amount;
    request.payload["notifyUrl"] = notifyUrl;
    request.payload["NotifyURL"] = notifyUrl;
    sendRequest(request, RequestKind::CreateQr, kPaymentRequestTimeoutMs);
}

void PaymentNetworkManager::startMedicalInsurancePay(const QString& billNo, double amount, const QString& paymentToken)
{
    common::Request request;
    request.module = "billing";
    request.action = "medicalInsurancePay";
    request.payload["账单号"] = billNo;
    request.payload["billNo"] = billNo;
    request.payload["支付方式"] = "本地医保统筹仿真支付";
    request.payload["amount"] = amount;
    if (!paymentToken.trimmed().isEmpty()) {
        request.payload["paymentToken"] = paymentToken.trimmed();
    }
    sendRequest(request, RequestKind::MedicalInsurance, kMedicalRequestTimeoutMs);
    m_medicalSlowTimer->start(kMedicalSlowHintMs);
}

void PaymentNetworkManager::payBill(const QString& billNo, double amount, const QString& paymentToken, const QString& payMethod)
{
    m_confirmationBillNo = billNo.trimmed();
    common::Request request;
    request.module = "billing";
    request.action = "pay";
    request.payload["账单号"] = billNo;
    request.payload["billNo"] = billNo;
    request.payload["合计"] = amount;
    request.payload["amount"] = amount;
    request.payload["支付方式"] = payMethod.isEmpty() ? "微信/支付宝扫码" : payMethod;
    if (!paymentToken.trimmed().isEmpty()) {
        request.payload["paymentToken"] = paymentToken.trimmed();
    }
    sendRequest(request, RequestKind::SelfPay, kSelfPayRequestTimeoutMs);
}

void PaymentNetworkManager::checkPayStatus(const QString& billNo, bool forceServer)
{
    if (forceServer && !billNo.trimmed().isEmpty() && m_confirmationBillNo.isEmpty()) {
        m_confirmationBillNo = billNo.trimmed();
    }
    if (kForceMockPaymentFlow && !forceServer) {
        const QJsonObject data = mockPaidStatusResponse(billNo, 0.0);
        QTimer::singleShot(0, this, [this, data]() {
            emit paymentStatusReady(data);
        });
        return;
    }

    common::Request request;
    request.module = "billing";
    request.action = "checkPayStatus";
    request.payload["billNo"] = billNo;
    sendRequest(request, RequestKind::CheckStatus, kPaymentRequestTimeoutMs);
}

bool PaymentNetworkManager::hasPendingRequest() const
{
    return m_pendingKind != RequestKind::None;
}

void PaymentNetworkManager::cancelPendingRequest()
{
    finishRequest();
    m_confirmationBillNo.clear();
}

void PaymentNetworkManager::onResponseReceived(const common::Response& response)
{
    if (response.data.value("module").toString() != "billing") {
        return;
    }
    if (m_pendingKind == RequestKind::None) {
        handleLatePaymentConfirmation(response);
        return;
    }

    const QString action = response.data.value("action").toString();
    qInfo().noquote()
        << "PaymentResponse"
        << "action=" + action
        << "expected=" + actionForKind(m_pendingKind)
        << "billNo=" + response.data.value("billNo").toString()
        << "success=" + QString(response.success ? "true" : "false")
        << "message=" + response.message;
    if (action != actionForKind(m_pendingKind)) {
        handleLatePaymentConfirmation(response);
        return;
    }

    const QString responseBillNo = response.data.value("billNo").toString();
    if (!responseBillNo.isEmpty() && !m_pendingBillNo.isEmpty() && responseBillNo != m_pendingBillNo) {
        handleLatePaymentConfirmation(response);
        return;
    }

    const RequestKind kind = m_pendingKind;
    m_timeoutTimer->stop();
    if (kind == RequestKind::MedicalInsurance) {
        m_medicalSlowTimer->stop();
    }

    if (!response.success) {
        fail(kind,
             friendlyMessageForAction(action, response.message),
             response.message,
             "SERVER_ERROR");
        return;
    }

    PaymentError validationError;
    const bool valid = kind == RequestKind::CreateQr
        ? validateQrResponse(response.data, &validationError)
        : kind == RequestKind::MedicalInsurance
            ? validateInsuranceResponse(response.data, &validationError)
            : validateStatusResponse(response.data, &validationError);
    if (!valid) {
        fail(kind, validationError.userMessage, validationError.technicalMessage, validationError.errorCode);
        return;
    }

    finishRequest();
    if (kind == RequestKind::CreateQr) {
        emit qrReady(response.data);
    } else if (kind == RequestKind::MedicalInsurance) {
        emit medicalInsuranceReady(response.data);
    } else {
        emit paymentStatusReady(response.data);
    }
}

void PaymentNetworkManager::onSocketError(const QString& message)
{
    if (m_pendingKind == RequestKind::None) {
        return;
    }
    fail(m_pendingKind, "网络连接异常，请检查网络后重试。", message, "NETWORK_ERROR");
}

void PaymentNetworkManager::onRequestTimeout()
{
    if (m_pendingKind == RequestKind::None) {
        return;
    }

    const RequestKind kind = m_pendingKind;
    if (kind == RequestKind::SelfPay) {
        qWarning().noquote()
            << "Warning: Self-pay confirmation timed out; entering automatic status polling.";
        const QString billNo = m_pendingBillNo;
        finishRequest();
        emit paymentConfirmationDelayed(billNo);
        return;
    }

    if (kind == RequestKind::CheckStatus) {
        const QString billNo = m_pendingBillNo;
        if (!m_confirmationBillNo.isEmpty() && billNo == m_confirmationBillNo) {
            qWarning().noquote()
                << "Warning: Payment status confirmation timed out while confirmation is still pending.";
            finishRequest();
            emit paymentConfirmationDelayed(billNo);
            return;
        }
        fail(kind,
             "支付状态确认超时，请检查服务端或数据库连接后重试。",
             "Payment status confirmation timed out",
             "TIMEOUT");
        return;
    }

    if (kMockPaymentOnTimeout) {
        qWarning().noquote() << "Warning: Payment timeout, switching to MOCK mode for debugging.";
        const QString billNo = m_pendingBillNo;
        const double amount = m_pendingAmount;
        const QString action = actionForKind(kind);

        finishRequest();
        if (kind == RequestKind::CheckStatus) {
            emit paymentStatusReady(mockPaidStatusResponse(billNo, amount));
            return;
        }

        const QJsonObject data = mockQrResponse(billNo, amount, action, QString());
        emit qrReady(data);
        return;
    }

    const TimeoutException exception("网络连接超时，请检查网络后重试");
    fail(m_pendingKind,
         QString::fromUtf8(exception.what()),
         QString("TimeoutException: 支付服务响应超时，%1 timed out after hard limit").arg(actionForKind(m_pendingKind)),
         "TIMEOUT");
}

void PaymentNetworkManager::onMedicalSlowTimeout()
{
    if (m_pendingKind == RequestKind::MedicalInsurance) {
        emit medicalInsuranceSlow();
    }
}

void PaymentNetworkManager::sendRequest(const common::Request& request, RequestKind kind, int timeoutMs)
{
    if (hasPendingRequest()) {
        fail(m_pendingKind, "已有支付请求正在处理中，请稍后再试。", "Duplicate payment request blocked", "DUPLICATE_REQUEST");
        return;
    }

    m_pendingKind = kind;
    m_pendingBillNo = request.payload.value("billNo").toString(request.payload.value("账单号").toString());
    m_pendingAmount = amountFromPayload(request.payload);
    qInfo().noquote()
        << "PaymentRequest"
        << "action=" + request.action
        << "billNo=" + m_pendingBillNo
        << "amount=" + QString::number(m_pendingAmount, 'f', 2)
        << "connected=" + QString(m_apiClient->isConnected() ? "true" : "false")
        << "timeoutMs=" + QString::number(timeoutMs);
    m_timeoutTimer->start(timeoutMs);
    if (!m_apiClient->send(request)) {
        fail(kind, "支付请求发送失败，请检查服务端连接。", "ApiClient::send returned false", "SEND_FAILED");
    }
}

void PaymentNetworkManager::finishRequest()
{
    m_timeoutTimer->stop();
    m_medicalSlowTimer->stop();
    m_pendingKind = RequestKind::None;
    m_pendingBillNo.clear();
    m_pendingAmount = 0.0;
}

void PaymentNetworkManager::fail(RequestKind kind,
                                 const QString& userMessage,
                                 const QString& technicalMessage,
                                 const QString& errorCode)
{
    const PaymentError error{
        actionForKind(kind),
        m_pendingBillNo,
        userMessage.isEmpty() ? "支付服务暂时不可用，请稍后重试。" : userMessage,
        technicalMessage,
        errorCode
    };
    appendPaymentLog(error);
    finishRequest();
    emit requestFailed(error);
}

bool PaymentNetworkManager::handleLatePaymentConfirmation(const common::Response& response)
{
    const QString action = response.data.value("action").toString();
    if (action != "pay" && action != "checkPayStatus") {
        return false;
    }

    const QString billNo = response.data.value("billNo").toString();
    if (billNo.isEmpty() || m_confirmationBillNo.isEmpty() || billNo != m_confirmationBillNo) {
        return false;
    }
    if (!response.success) {
        return false;
    }
    if (response.data.value("paymentStatus").toString() == "PAID") {
        finishRequest();
        m_confirmationBillNo.clear();
        emit paymentStatusReady(response.data);
        return true;
    }
    return false;
}

bool PaymentNetworkManager::validateQrResponse(const QJsonObject& data, PaymentError* error) const
{
    if (data.value("billNo").toString().isEmpty()
        || data.value("qrPayload").toString().isEmpty()
        || data.value("qrImageBase64").toString().isEmpty()) {
        if (error) {
            *error = {"createPaymentQr", m_pendingBillNo, "支付二维码数据异常，请重试。", "Missing qrPayload or qrImageBase64", "INVALID_QR_RESPONSE"};
        }
        return false;
    }
    return true;
}

bool PaymentNetworkManager::validateInsuranceResponse(const QJsonObject& data, PaymentError* error) const
{
    if (data.value("billNo").toString().isEmpty()
        || data.value("paymentStatus").toString().isEmpty()) {
        if (error) {
            *error = {"medicalInsurancePay", m_pendingBillNo, "医保服务暂时不可用，请稍后重试。", "Missing billNo or paymentStatus", "INVALID_INSURANCE_RESPONSE"};
        }
        return false;
    }
    return true;
}

bool PaymentNetworkManager::validateStatusResponse(const QJsonObject& data, PaymentError* error) const
{
    if (data.value("paymentStatus").toString().isEmpty()) {
        if (error) {
            *error = {"checkPayStatus", m_pendingBillNo, "支付状态返回异常，请刷新订单后重试。", "Missing paymentStatus", "INVALID_STATUS_RESPONSE"};
        }
        return false;
    }
    return true;
}

QString PaymentNetworkManager::actionForKind(RequestKind kind) const
{
    switch (kind) {
    case RequestKind::CreateQr:
        return "createPaymentQr";
    case RequestKind::MedicalInsurance:
        return "medicalInsurancePay";
    case RequestKind::SelfPay:
        return "pay";
    case RequestKind::CheckStatus:
        return "checkPayStatus";
    case RequestKind::None:
        return {};
    }
    return {};
}

QString PaymentNetworkManager::friendlyMessageForAction(const QString& action, const QString& message) const
{
    if (action == "medicalInsurancePay") {
        return message.isEmpty() ? "医保服务暂时不可用，请稍后重试。" : message;
    }
    if (action == "createPaymentQr") {
        return message.isEmpty() ? "支付二维码生成失败，请稍后重试。" : message;
    }
    if (action == "checkPayStatus") {
        return message.isEmpty() ? "支付状态刷新失败，请稍后重试。" : message;
    }
    return message.isEmpty() ? "支付服务暂时不可用，请稍后重试。" : message;
}

void PaymentNetworkManager::appendPaymentLog(const PaymentError& error) const
{
    const QString line = QString("[%1] action=%2 billNo=%3 code=%4 userMessage=%5 detail=%6\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
             error.action,
             error.billNo,
             error.errorCode,
             error.userMessage,
             error.technicalMessage);

    QFile file(logFilePath());
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << line;
    }
    qWarning().noquote() << "PaymentError" << line.trimmed();
}

} // namespace hospital::client
