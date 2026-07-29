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
    const std::string billing = readFile("server/src/modules/BillingService.cpp");
    if (!containsAll(billing, {
            "processSelfPay",
            "SELECT b.id, b.patient_id, b.total_amount, b.status",
            "clientAmount",
            "UPDATE bills SET status = 'PAID'",
            "AND status IN ('UNPAID', 'PENDING', 'PENDING_PAYMENT')",
            "numRowsAffected() != 1",
            "callMedicalInsuranceAPI",
            "handleInsuranceCallback",
            "INSURANCE_PROCESSING",
            "LOCAL_REGISTRATION_INSURANCE",
            "本地医保统筹仿真支付成功",
            "INSERT INTO operation_logs"
        })) {
        return 1;
    }
    if (billing.find("UPDATE doctor_schedules SET remain_quota = remain_quota") != std::string::npos) {
        return 7;
    }

    if (!containsAll(billing, {
            "createPaymentQr",
            "checkPayStatus",
            "mockPay",
            "assertBillVisibleToSession",
            "qrImageBase64",
            "qrPayload",
            "simulationMode",
            "paymentStatus",
            "r.operator_id = :operator_user_id"
        })) {
        return 4;
    }

    const std::string registration = readFile("server/src/modules/RegistrationService.cpp");
    if (!containsAll(registration, {
            "orderHistoryInDatabase",
            "registrationHistory",
            "r.operator_id = :operator_user_id",
            "ORDER BY r.register_time DESC",
            "billNo",
            "paymentStatus",
            "totalAmount"
        })) {
        return 5;
    }

    const std::string protocolHeader = readFile("common/include/common/Protocol.h");
    const std::string protocolSource = readFile("common/src/Protocol.cpp");
    const std::string apiClient = readFile("client/src/ApiClient.cpp");
    if (!containsAll(protocolHeader + protocolSource + apiClient, {
            "headers",
            "Authorization",
            "Bearer "
        })) {
        return 6;
    }

    const std::string router = readFile("server/src/RequestRouter.cpp");
    const std::string connection = readFile("server/src/ClientConnection.cpp");
    if (!containsAll(router + connection, {
            "request.module == \"billing\" && request.action == \"mockPay\"",
            "handleHttpMockPay",
            "/mock-pay",
            "\"/p\"",
            "\"/p-status\"",
            "queryItemValue(\"b\")",
            "queryItemValue(\"t\")",
            "__mockPay",
            "paymentToken",
            "rememberScannedPayment",
            "scanPaymentStatus",
            "200 OK",
            "支付成功"
        })) {
        return 8;
    }
    if (connection.find("request.payload[\"amount\"]") != std::string::npos
        || connection.find("request.payload[\"合计\"]") != std::string::npos) {
        return 9;
    }
    if (!containsAll(billing, {
            "const bool isMockPay",
            "payload.value(\"__mockPay\").toBool(false)",
            "if (!isMockPay &&",
            "支付金额缺失"
        })) {
        return 10;
    }

    const std::string paymentDialogHeader = readFile("client/include/client/PaymentSelectionDialog.h");
    const std::string paymentDialogSource = readFile("client/src/PaymentSelectionDialog.cpp");
    const std::string paymentNetworkHeader = readFile("client/include/client/PaymentNetworkManager.h");
    const std::string paymentNetworkSource = readFile("client/src/PaymentNetworkManager.cpp");
    const std::string windowHeader = readFile("client/include/client/PatientAppointmentWindow.h");
    const std::string windowSource = readFile("client/src/PatientAppointmentWindow.cpp");
    if (!containsAll(paymentDialogHeader + paymentNetworkHeader + windowHeader, {
            "PaymentSelectionDialog",
            "PaymentNetworkManager",
            "TimeoutException",
            "onDirectPaymentClicked",
            "onMedicalInsurancePaymentClicked",
            "requestOrderHistory",
            "paymentCompleted"
        })) {
        return 2;
    }
    if (!containsAll(paymentDialogSource + paymentNetworkSource + windowSource, {
            "PaymentSelectionDialog",
            "setFixedSize(520, 660)",
            "QTimer",
            "QProgressBar",
            "checkPayStatus",
            "createPaymentQr",
            "QTimer::singleShot(200, this, &PaymentSelectionDialog::onMedicalInsurancePaymentClicked)",
            "手机无法打开时，点击本机模拟扫码支付",
            "simulateLocalScanPayment",
            "微信/支付宝扫码（本机仿真）",
            "加载中",
            "重试",
            "自费支付",
            "医保统筹",
            "m_registrationInsuranceApproved",
            "需先在挂号页选择医保统筹并通过资格校验",
            "挂号医保资格已通过，扫码支付报销后个人自付金额",
            "去支付",
            "直接支付",
            "onDirectPaymentClicked",
            "onMedicalInsurancePaymentClicked",
            "PaymentNetworkManager",
            "failCurrentRequest",
            "支付服务响应超时",
            "网络连接超时，请检查网络后重试",
            "appendPaymentLog",
            "validateQrResponse",
            "NotifyURL",
            "OrderID",
            "Amount",
            "8000",
            "3000"
        })) {
        return 3;
    }
    if (paymentDialogSource.find("正在连接医保专网") != std::string::npos
        || paymentDialogSource.find("无法内嵌医保 H5 支付页面") != std::string::npos
        || paymentDialogSource.find("无需手机扫码") != std::string::npos
        || paymentDialogSource.find("startMedicalInsurancePay(m_billNo, m_totalAmount, m_paymentToken)") != std::string::npos
        || paymentNetworkSource.find("Missing or invalid h5PaymentUrl") != std::string::npos
        || billing.find("medical-insurance.example") != std::string::npos
        || billing.find("等待医保局异步回调") != std::string::npos) {
        return 11;
    }

    return 0;
}
