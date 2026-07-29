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
            "registration_insurance_check",
            "insu_status",
            "valid_end_date",
            "is_remote_filed",
            "arrears_months",
            "benefit_suspended",
            "insurance_type",
            "outpatient_pooling_supported",
            "annual_quota_total",
            "annual_quota_used",
            "quota_year",
            "data_version",
            "check_enabled"
        })) {
        return 1;
    }

    if (!containsAll(schema, {
            "110101199603180021",
            "110101198811020033",
            "110101201807090044",
            "110101197205060055",
            "110101199112120066",
            "110101200102030077"
        })) {
        return 2;
    }

    const std::string database = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(database, {
            "CREATE TABLE IF NOT EXISTS registration_insurance_check",
            "CREATE TABLE IF NOT EXISTS registration_insurance_tokens",
            "CREATE TABLE IF NOT EXISTS registration_insurance_audit_logs",
            "ALTER TABLE registrations ADD COLUMN insurance_result_code",
            "ALTER TABLE registrations ADD COLUMN insurance_token_no",
            "ALTER TABLE registrations ADD COLUMN payment_identity"
        })) {
        return 3;
    }

    const std::string registration = readFile("server/src/modules/RegistrationService.cpp");
    if (!containsAll(registration, {
            "REG_INS_001",
            "REG_INS_002",
            "REG_INS_003",
            "REG_INS_004",
            "REG_INS_005",
            "REG_INS_006",
            "REG_INS_007",
            "REG_INS_008",
            "insurancePrecheck",
            "insuranceToken",
            "dataVersion",
            "DATE_ADD(NOW(), INTERVAL 5 MINUTE)",
            "used_at IS NULL",
            "UPDATE registration_insurance_tokens SET used_at = NOW()",
            "registration_insurance_audit_logs",
            "paymentIdentity",
            "医保统筹资格已通过"
        })) {
        return 4;
    }

    if (!containsAll(registration, {
            "request.action == \"insuranceProfile\"",
            "request.action == \"saveInsuranceProfile\"",
            "common::Response registrationInsuranceProfile",
            "common::Response saveRegistrationInsuranceProfile",
            "INSERT INTO registration_insurance_check",
            "ON DUPLICATE KEY UPDATE",
            "insuredAreaMode",
            "request.payload[\"insuranceType\"]",
            "request.payload[\"validEndDate\"]",
            "request.payload[\"annualQuotaUsed\"]",
            "request.payload[\"annualQuotaTotal\"]"
        })) {
        return 8;
    }

    if (registration.find("processMedicalInsurancePay") != std::string::npos) {
        return 5;
    }

    if (!containsAll(registration, {
            "struct InsuranceSettlement",
            "calculateInsuranceSettlement",
            "policyScopeAmount",
            "reimbursementAmount",
            "selfPayAmount",
            "deductible",
            "selfPayRatioAmount",
            "overLimitSelfPayAmount",
            "annualQuotaRemaining",
            "URBAN_EMPLOYEE",
            "URBAN_RESIDENT",
            "const InsuranceSettlement settlement = calculateInsuranceSettlement(registrationFee, record)",
            "query.bindValue(\":total_amount\", settlement.selfPayAmount)",
            "data[\"originalAmount\"] = registrationFee",
            "data[\"insuranceReimbursementAmount\"] = settlement.reimbursementAmount",
            "data[\"totalAmount\"] = settlement.selfPayAmount"
        })) {
        return 10;
    }

    const std::string windowHeader = readFile("client/include/client/PatientAppointmentWindow.h");
    const std::string windowSource = readFile("client/src/PatientAppointmentWindow.cpp");
    const std::string paymentDialogHeader = readFile("client/include/client/PaymentSelectionDialog.h");
    const std::string paymentDialogSource = readFile("client/src/PaymentSelectionDialog.cpp");
    if (!containsAll(windowHeader + windowSource, {
            "m_paymentModeBox",
            "m_insuranceStatusLabel",
            "m_addAppointmentButton",
            "m_insuranceToken",
            "m_insuranceDataVersion",
            "m_pendingRegistrationInsuranceApproved",
            "requestInsurancePrecheck",
            "requestInsuranceProfile",
            "openInsuranceProfileDialog",
            "saveInsuranceProfile",
            "resetInsuranceCheck",
            "setInsuranceChecking",
            "医保统筹",
            "医保信息",
            "未绑定医保信息",
            "医保信息已保存",
            "医保资格校验中",
            "本次挂号无法使用医保统筹",
            "insurancePrecheck",
            "insuranceToken",
            "paymentMethod",
            "insuranceDataVersion"
        })) {
        return 6;
    }

    if (!containsAll(windowHeader + windowSource, {
            "QPushButton* m_insuranceProfileButton",
            "void requestInsuranceProfile()",
            "void openInsuranceProfileDialog(",
            "void saveInsuranceProfile(",
            "m_insuranceProfileButton = new QPushButton(\"医保信息\"",
            "connect(m_insuranceProfileButton, &QPushButton::clicked, this, &PatientAppointmentWindow::requestInsuranceProfile)",
            "request.action = \"insuranceProfile\"",
            "request.action = \"saveInsuranceProfile\"",
            "payload[\"insuredAreaMode\"]",
            "payload[\"insuranceType\"]",
            "payload[\"validEndDate\"]",
            "payload[\"annualQuotaUsed\"]",
            "payload[\"annualQuotaTotal\"]",
            "未绑定医保信息",
            "医保信息已保存"
        })) {
        return 9;
    }

    if (!containsAll(paymentDialogHeader + paymentDialogSource, {
            "m_registrationInsuranceApproved",
            "setEnabled(m_registrationInsuranceApproved)",
            "需先在挂号页选择医保统筹并通过资格校验",
            "挂号医保资格已通过，扫码支付报销后个人自付金额",
            "本次挂号未通过医保统筹资格校验"
        })) {
        return 7;
    }

    return 0;
}
