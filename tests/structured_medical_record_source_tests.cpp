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
            "present_illness VARCHAR",
            "past_history VARCHAR",
            "physical_sign VARCHAR",
            "icd_code VARCHAR",
            "external_report_hospital VARCHAR",
            "external_report_type VARCHAR",
            "external_report_date DATE",
            "external_report_summary VARCHAR",
            "external_report_conclusion VARCHAR",
            "external_report_attachment VARCHAR"
        })) {
        return 1;
    }

    const std::string cmake = readFile("client/CMakeLists.txt");
    if (!containsAll(cmake, {
            "PrintSupport"
        })) {
        return 2;
    }

    const std::string consultationPage = readFile("client/src/pages/ConsultationPage.cpp");
    if (!containsAll(consultationPage, {
            "#include <QPrinter>",
            "#include <QScrollArea>",
            "#include <QScreen>",
            "#include <QTabWidget>",
            "#include <QTextDocument>",
            "availableGeometry",
            "validateConsultationDraft",
            "内科模板",
            "骨科模板",
            "儿科模板",
            "ICD诊断",
            "调取历史病历",
            "打印病历",
            "导出PDF",
            "外院报告",
            "导入外院报告",
            "来源医院",
            "报告类型",
            "报告日期",
            "报告摘要",
            "报告结论",
            "附件路径",
            "现病史",
            "既往史",
            "体格检查",
            "ICD编码",
            "renderMedicalRecordHtml",
            "applyMedicalRecordTemplate"
        })) {
        return 3;
    }

    const std::string consultationService = readFile("server/src/modules/ConsultationService.cpp");
    if (!containsAll(consultationService, {
            "present_illness",
            "past_history",
            "physical_sign",
            "icd_code",
            "external_report_hospital",
            "external_report_type",
            "external_report_date",
            "external_report_summary",
            "external_report_conclusion",
            "external_report_attachment",
            "payload.value(\"现病史\")",
            "payload.value(\"既往史\")",
            "payload.value(\"体格检查\")",
            "payload.value(\"ICD编码\")",
            "payload.value(\"外院报告医院\")",
            "payload.value(\"外院报告类型\")",
            "payload.value(\"外院报告日期\")",
            "payload.value(\"外院报告摘要\")",
            "payload.value(\"外院报告结论\")",
            "payload.value(\"外院报告附件\")",
            "AS '现病史'",
            "AS 'ICD编码'",
            "AS '外院报告医院'",
            "AS '外院报告结论'"
        })) {
        return 4;
    }

    const std::string recordService = readFile("server/src/modules/PatientRecordService.cpp");
    if (!containsAll(recordService, {
            "present_illness",
            "past_history",
            "physical_sign",
            "icd_code",
            "external_report_hospital",
            "external_report_type",
            "external_report_date",
            "external_report_summary",
            "external_report_conclusion",
            "external_report_attachment",
            "AS '现病史'",
            "AS '既往史'",
            "AS '体格检查'",
            "AS 'ICD编码'",
            "AS '外院报告医院'",
            "AS '外院报告结论'"
        })) {
        return 5;
    }

    const std::string demo = readFile("server/src/DemoRepository.cpp");
    if (!containsAll(demo, {
            "现病史",
            "既往史",
            "体格检查",
            "ICD编码",
            "外院报告医院",
            "外院报告类型",
            "外院报告日期",
            "外院报告摘要",
            "外院报告结论",
            "外院报告附件"
        })) {
        return 6;
    }

    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(modulePage, {
            "\"现病史\", \"既往史\", \"体格检查\", \"ICD编码\"",
            "\"外院报告医院\", \"外院报告类型\", \"外院报告日期\", \"外院报告结论\""
        })) {
        return 7;
    }

    return 0;
}
