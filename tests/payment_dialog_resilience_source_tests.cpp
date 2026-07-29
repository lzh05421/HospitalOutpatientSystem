#include <fstream>
#include <iterator>
#include <string>
#include <vector>

std::string readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

bool containsAll(const std::string& source, const std::vector<std::string>& fragments)
{
    for (const auto& fragment : fragments) {
        if (source.find(fragment) == std::string::npos) {
            return false;
        }
    }
    return true;
}

int main()
{
    const std::string network = readFile("client/src/PaymentNetworkManager.cpp");
    const std::string dialog = readFile("client/src/PaymentSelectionDialog.cpp");
    const std::string header = readFile("client/include/client/PaymentNetworkManager.h")
        + readFile("client/include/client/PaymentSelectionDialog.h");

    if (!containsAll(header + network, {
            "PaymentNetworkManager",
            "TimeoutException",
            "payBill",
            "kSelfPayRequestTimeoutMs",
            "forceServer",
            "m_confirmationBillNo",
            "handleLatePaymentConfirmation",
            "paymentConfirmationDelayed",
            "requestFailed",
            "NETWORK_ERROR",
            "SERVER_ERROR",
            "TIMEOUT",
            "DUPLICATE_REQUEST",
            "INVALID_QR_RESPONSE",
            "INVALID_INSURANCE_RESPONSE",
            "INVALID_STATUS_RESPONSE",
            "appendPaymentLog"
        })) {
        return 1;
    }

    if (!containsAll(dialog, {
            "QProgressBar",
            "PaymentUiState::LoadingQr",
            "PaymentUiState::ConfirmingPayment",
            "PaymentUiState::ConfirmationSlow",
            "PaymentUiState::Error",
            "微信/支付宝扫一扫",
            "paymentToken",
            "serverPaymentCallbackUrl",
            "scanPaymentStatusUrl",
            "/p",
            "/p-status",
            "kMaxMockQrPayloadBytes",
            "startConfirmationPolling",
            "pollScanPaymentStatus",
            "m_confirmationAttempts",
            "kMaxLoadingConfirmationAttempts",
            "若手机无法加载网页，可点击下方本机模拟扫码支付",
            "手机无法打开时，点击本机模拟扫码支付",
            "simulateLocalScanPayment",
            "微信/支付宝扫码（本机仿真）",
            "QTimer::singleShot(200, this, &PaymentSelectionDialog::onMedicalInsurancePaymentClicked)",
            "重试",
            "ErrorHandler",
            "网络连接超时，请检查网络后重试",
            "m_registrationInsuranceApproved",
            "需先在挂号页选择医保统筹并通过资格校验",
            "挂号医保资格已通过，扫码支付报销后个人自付金额",
            "本次挂号未通过医保统筹资格校验"
        })) {
        return 2;
    }

    if (network.find("支付成功：你已支付挂号费") != std::string::npos) {
        return 3;
    }
    if (!containsAll(network, {
            "kind == RequestKind::SelfPay",
            "emit paymentConfirmationDelayed(billNo)",
            "Self-pay confirmation timed out; entering automatic status polling"
        })) {
        return 5;
    }
    if (!containsAll(network, {
            "kind == RequestKind::CheckStatus",
            "emit paymentConfirmationDelayed(billNo)",
            "Payment status confirmation timed out while confirmation is still pending"
        })) {
        return 7;
    }
    if (!containsAll(network, {
            "m_pendingKind == RequestKind::None",
            "action != actionForKind(m_pendingKind)",
            "handleLatePaymentConfirmation(response)",
            "response.data.value(\"paymentStatus\").toString() == \"PAID\"",
            "emit paymentStatusReady(response.data)"
        })) {
        return 9;
    }
    if (network.find("emit qrReady(mockQrResponse(billNo, amount, action, QString()))") != std::string::npos) {
        return 6;
    }
    if (dialog.find("startMockPaymentCallbackServer") != std::string::npos
        || dialog.find("handleMockPaymentCallback") != std::string::npos
        || dialog.find("m_mockCallbackServer->listen") != std::string::npos) {
        return 10;
    }
    if (dialog.find("query.addQueryItem(\"billNo\"") != std::string::npos
        || dialog.find("query.addQueryItem(\"amount\"") != std::string::npos
        || dialog.find("query.addQueryItem(\"token\"") != std::string::npos) {
        return 11;
    }
    if (dialog.find("if (isMock)") != std::string::npos
        && dialog.find("m_pollTimer->stop();") != std::string::npos) {
        return 12;
    }
    if (!containsAll(dialog, {
            "showQrImage",
            "startConfirmationPolling(\"请用手机微信或支付宝扫一扫；若手机无法加载网页，可点击下方本机模拟扫码支付。\")",
            "本机模拟扫码支付"
        })) {
        return 13;
    }
    if (dialog.find("startAutoConfirmFallback") != std::string::npos
        || dialog.find("autoConfirmIfStillWaiting") != std::string::npos
        || dialog.find("m_autoConfirmTimer") != std::string::npos) {
        return 14;
    }
    if (!containsAll(dialog, {
            "QNetworkAccessManager",
            "QNetworkReply",
            "m_scanStatusManager",
            "m_scanStatusReply",
            "pollScanPaymentStatus()",
            "scanPaymentStatusUrl()",
            "data.value(\"paymentStatus\").toString() == \"PAID\"",
            "finishAsPaid(\"支付成功。\")"
        })) {
        return 15;
    }
    if (dialog.find("正在连接医保专网") != std::string::npos
        || dialog.find("openInsuranceWebView(data.value(\"h5PaymentUrl\").toString())") != std::string::npos
        || dialog.find("无法内嵌医保 H5 支付页面") != std::string::npos
        || dialog.find("无需手机扫码") != std::string::npos
        || dialog.find("startMedicalInsurancePay(m_billNo, m_totalAmount, m_paymentToken)") != std::string::npos
        || dialog.find("PaymentUiState::LoadingInsurance") != std::string::npos
        || dialog.find("PaymentUiState::InsuranceSlow") != std::string::npos
        || dialog.find("正在提交本地医保仿真支付") != std::string::npos) {
        return 17;
    }
    if (!containsAll(dialog, {
            "m_lastAction = \"direct\"",
            "m_networkManager->createPaymentQr(m_billNo, m_totalAmount, serverPaymentCallbackUrl())",
            "正在生成医保统筹个人自付金额支付二维码"
        })) {
        return 18;
    }
    if (!containsAll(dialog, {
            "if (m_lastAction == \"direct\"",
            "pollScanPaymentStatus();",
            "return;",
            "m_networkManager->checkPayStatus(m_billNo, true)"
        })) {
        return 16;
    }
    if (!containsAll(dialog, {
            "canKeepConfirming",
            "error.action == \"pay\" || error.action == \"checkPayStatus\"",
            "startConfirmationPolling(\"支付结果仍在后台确认，系统会继续自动刷新...\")",
            "return;"
        })) {
        return 8;
    }

    const std::string window = readFile("client/src/PatientAppointmentWindow.cpp");
    if (!containsAll(window, {
            "m_pendingRegistrationInsuranceApproved",
            "PaymentSelectionDialog dialog(m_apiClient,"
        })) {
        return 4;
    }

    return 0;
}
