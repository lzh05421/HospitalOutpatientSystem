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
    const std::string records = readFile("server/src/modules/PatientRecordService.cpp");
    const std::string registration = readFile("server/src/modules/RegistrationService.cpp");
    const std::string consultation = readFile("server/src/modules/ConsultationService.cpp");
    const std::string examination = readFile("server/src/modules/ExaminationService.cpp");
    const std::string prescription = readFile("server/src/modules/PrescriptionService.cpp");
    const std::string demoRepository = readFile("server/src/DemoRepository.cpp");
    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");

    if (!containsAll(records, {
            "p.id_card AS '身份证号'",
            "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%')",
            "params.insert(\"keyword_id_card\", keyword)"
        })) {
        return 1;
    }

    if (!containsAll(registration, {
            "p.id_card AS '身份证号'",
            "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%')",
            "params.insert(\"keyword_id_card\", keyword)"
        })) {
        return 2;
    }

    if (!containsAll(consultation, {
            "p.id_card AS '身份证号'",
            "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%')",
            "params.insert(\"keyword_id_card\", keyword)"
        })) {
        return 3;
    }

    if (!containsAll(examination, {
            "p.id_card AS '身份证号'",
            "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%')",
            "params.insert(\"keyword_id_card\", keyword)"
        })) {
        return 4;
    }

    if (!containsAll(prescription, {
            "p.id_card AS '身份证号'",
            "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%')",
            "params.insert(\"keyword_id_card\", keyword)"
        })) {
        return 5;
    }

    if (!containsAll(demoRepository, {
            "patientFieldByName",
            "object[\"身份证号\"] = patientFieldByName",
            "row[\"身份证号\"] = patient.value(\"身份证号\").toString()",
            "row[\"身份证号\"] = patientFieldByName",
            "sortedScheduleRows"
        })) {
        return 6;
    }

    if (!containsAll(modulePage, {
            "\"患者\", \"身份证号\"",
            "\"患者编号\", \"患者\", \"身份证号\", \"电话\"",
            "\"挂号单号\", \"患者\", \"身份证号\"",
            "\"处方号\", \"挂号单号\", \"患者\", \"身份证号\"",
            "\"检查单号\", \"挂号单号\", \"患者\", \"身份证号\""
        })) {
        return 7;
    }

    return 0;
}
