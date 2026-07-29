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
    const std::string schema = readFile("database/schema.sql");
    if (!containsAll(database + schema, {
            "CREATE TABLE IF NOT EXISTS examination_items",
            "item_code VARCHAR",
            "item_name VARCHAR",
            "unit_price DECIMAL",
            "ALTER TABLE examinations ADD COLUMN item_id",
            "ALTER TABLE examinations ADD COLUMN report_finding",
            "ALTER TABLE examinations ADD COLUMN report_conclusion",
            "ALTER TABLE examinations ADD COLUMN report_attachment"
        })) {
        return 1;
    }

    const std::string service = readFile("server/src/modules/ExaminationService.cpp");
    if (!containsAll(service, {
            "saveExaminationItemInDatabase",
            "disableExaminationItemInDatabase",
            "request.action == \"items\"",
            "request.action == \"saveItem\"",
            "request.action == \"deleteItem\"",
            "SELECT id, unit_price, item_name FROM examination_items",
            "SELECT examination_no FROM examinations",
            "registration_id = :registration_id",
            "item_id = :item_id",
            "item_name = :item_name",
            "status = 'PENDING'",
            "该挂号单已有待检查的相同检查项目，请勿重复开立。",
            "UPDATE bills SET other_fee = other_fee + :exam_fee",
            "total_amount = total_amount + :exam_fee",
            "report_finding",
            "report_conclusion",
            "report_attachment",
            "报告所见",
            "报告结论",
            "报告附件",
            "检查项目字典中未找到该项目"
        })) {
        return 2;
    }

    const std::string consultation = readFile("server/src/modules/ConsultationService.cpp");
    if (!containsAll(consultation, {
            "本院检查报告",
            "GROUP_CONCAT",
            "report_conclusion",
            "examinations ex",
            "AS '本院检查报告'"
        })) {
        return 3;
    }

    const std::string examinationPage = readFile("client/src/pages/ExaminationPage.cpp");
    const std::string consultationPage = readFile("client/src/pages/ConsultationPage.cpp");
    const std::string pagesHeader = readFile("client/include/client/pages/Pages.h");
    if (!containsAll(examinationPage + pagesHeader, {
            "维护检查项目",
            "loadExaminationItems",
            "saveExaminationItem",
            "deleteExaminationItem",
            "onExaminationResponse",
            "QComboBox",
            "QDoubleSpinBox",
            "检查项目",
            "单价",
            "报告所见",
            "报告结论",
            "报告附件",
            "request.action = \"items\"",
            "request.action = \"saveItem\"",
            "request.action = \"deleteItem\""
        })) {
        return 4;
    }
    if (!containsAll(consultationPage + pagesHeader, {
            "loadExaminationItems",
            "m_examinationItems",
            "request.action = \"items\"",
            "auto* examItemBox = new QComboBox",
            "examItemBox->addItem",
            "检查项目字典",
            "itemBoxCurrentName"
        })) {
        return 6;
    }
    if (consultationPage.find("auto* examItemEdit = new QLineEdit") != std::string::npos) {
        return 7;
    }

    const std::string auth = readFile("server/src/AuthorizationService.cpp");
    const std::string menus = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(auth + menus, {
            "examination:items",
            "examination:saveItem",
            "examination:deleteItem"
        })) {
        return 5;
    }

    return 0;
}
