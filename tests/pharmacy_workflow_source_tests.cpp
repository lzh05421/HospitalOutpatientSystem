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
    const std::string schema = readFile("database/schema.sql");
    if (!containsAll(schema, {
            "reviewer_id BIGINT",
            "review_time DATETIME",
            "reject_reason VARCHAR(500)",
            "dispense_user_id BIGINT",
            "dispense_time DATETIME",
            "return_user_id BIGINT",
            "return_time DATETIME",
            "return_reason VARCHAR(500)"
        })) {
        return 1;
    }

    const std::string prescription = readFile("server/src/modules/PrescriptionService.cpp");
    if (!containsAll(prescription, {
            "rejectPrescriptionInDatabase",
            "returnPrescriptionInDatabase",
            "request.action == \"reject\"",
            "request.action == \"return\"",
            "status = 'REJECTED'",
            "status = 'RETURNED'",
            "reject_reason",
            "return_reason",
            "change_type, quantity",
            "'RETURN'",
            "INSERT INTO operation_logs",
            "WHEN 'REJECTED' THEN '已驳回'",
            "WHEN 'RETURNED' THEN '已退药'"
        })) {
        return 2;
    }

    const std::string billing = readFile("server/src/modules/BillingService.cpp");
    if (!containsAll(billing, {
            "status = 'DISPENSED'",
            "请先完成药房退药",
            "INSERT INTO operation_logs",
            "退费拦截"
        })) {
        return 3;
    }

    const std::string demoHeader = readFile("server/include/server/DemoRepository.h");
    const std::string demoSource = readFile("server/src/DemoRepository.cpp");
    if (!containsAll(demoHeader + demoSource, {
            "rejectPrescription",
            "returnPrescription",
            "已驳回",
            "已退药",
            "驳回原因",
            "退药原因",
            "请先完成药房退药"
        })) {
        return 4;
    }

    const std::string prescriptionPage = readFile("client/src/pages/PrescriptionPage.cpp");
    const std::string pagesHeader = readFile("client/include/client/pages/Pages.h");
    if (!containsAll(prescriptionPage + pagesHeader, {
            "退药入库",
            "returnPrescription",
            "驳回原因",
            "request.action = \"reject\"",
            "request.action = \"return\"",
            "患者信息",
            "药品明细"
        })) {
        return 5;
    }

    return 0;
}
