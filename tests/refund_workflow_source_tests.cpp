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
    const std::string database = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(database, {
            "CREATE TABLE IF NOT EXISTS refund_requests",
            "bill_id BIGINT NOT NULL",
            "refund_no VARCHAR(64) NOT NULL UNIQUE",
            "reason VARCHAR(500) NOT NULL",
            "status VARCHAR(32) NOT NULL DEFAULT 'PENDING'",
            "requested_by BIGINT NULL",
            "reviewed_by BIGINT NULL",
            "review_note VARCHAR(500)"
        })) {
        return 1;
    }

    const std::string billing = readFile("server/src/modules/BillingService.cpp");
    if (!containsAll(billing, {
            "requestRefundBill",
            "reviewRefundBill",
            "request.action == \"requestRefund\"",
            "request.action == \"reviewRefund\"",
            "INSERT INTO refund_requests",
            "退费申请",
            "退费审核",
            "退费拒绝",
            "退费原因不能为空",
            "已提交退费申请，等待审核",
            "该账单存在已发药处方，请先完成药房退药后再审核退费",
            "status = 'DISPENSED'",
            "status = 'RETURNED'",
            "UPDATE bills SET status = 'REFUNDED'",
            "INSERT INTO operation_logs"
        })) {
        return 2;
    }
    if (billing.find("request.action == \"refund\"") == std::string::npos
        || billing.find("return requestRefundBill(request.payload)") == std::string::npos) {
        return 3;
    }

    const std::string billingPage = readFile("client/src/pages/BillingPage.cpp");
    const std::string pagesHeader = readFile("client/include/client/pages/Pages.h");
    if (!containsAll(billingPage + pagesHeader, {
            "申请退费",
            "审核退费",
            "refundSelectedBill",
            "reviewSelectedRefund",
            "hasPermission(\"billing:requestRefund\")",
            "hasPermission(\"billing:reviewRefund\")",
            "reviewRefundButton->setVisible",
            "request.action = \"requestRefund\"",
            "request.action = \"reviewRefund\"",
            "退费原因",
            "审核意见"
        })) {
        return 4;
    }
    if (billingPage.find("request.action = \"refund\"") != std::string::npos) {
        return 5;
    }

    const std::string auth = readFile("server/src/AuthorizationService.cpp");
    const std::string schema = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(auth + schema, {
            "billing:requestRefund",
            "billing:reviewRefund"
        })) {
        return 6;
    }
    const auto cashierPreset = auth.find("{\"CASHIER\", {");
    const auto adminPreset = auth.find("{\"ADMIN\", {");
    if (cashierPreset == std::string::npos || adminPreset == std::string::npos) {
        return 7;
    }
    const std::string cashierBlock = auth.substr(cashierPreset, adminPreset - cashierPreset);
    if (cashierBlock.find("billing:requestRefund") == std::string::npos
        || cashierBlock.find("billing:reviewRefund") != std::string::npos) {
        return 8;
    }
    const std::string schemaCashierGrant = "'billing:refund','billing:requestRefund','billing:update'";
    if (schema.find(schemaCashierGrant) == std::string::npos
        || schema.find("'billing:refund','billing:requestRefund','billing:reviewRefund','billing:update'") != std::string::npos) {
        return 9;
    }
    if (auth.find("\"billing:reviewRefund\"") == std::string::npos
        || schema.find("'billing:reviewRefund'") == std::string::npos) {
        return 10;
    }

    return 0;
}
