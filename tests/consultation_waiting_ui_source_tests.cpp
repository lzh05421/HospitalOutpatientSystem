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
    const std::string header = readFile("client/include/client/pages/Pages.h");
    if (!containsAll(header, {
            "void rowsUpdated(const QJsonArray& rows) override;",
            "QLabel* m_waitingCountLabel = nullptr;",
            "QLabel* m_calledCountLabel = nullptr;",
            "QLabel* m_emergencyCountLabel = nullptr;",
            "QLabel* m_pendingConsultationLabel = nullptr;",
            "QLabel* m_inConsultationLabel = nullptr;",
            "QLabel* m_reviewCountLabel = nullptr;"
        })) {
        return 1;
    }

    const std::string waitingPage = readFile("client/src/pages/WaitingQueuePage.cpp");
    if (!containsAll(waitingPage, {
            "createMetricCard",
            "createStatusChip",
            "m_waitingCountLabel",
            "m_calledCountLabel",
            "m_emergencyCountLabel",
            "m_averageWaitLabel",
            "rowsUpdated(const QJsonArray& rows)",
            "待叫号",
            "已叫号",
            "检查完成待复诊"
        })) {
        return 2;
    }

    const std::string consultationPage = readFile("client/src/pages/ConsultationPage.cpp");
    if (!containsAll(consultationPage, {
            "createMetricCard",
            "createStatusChip",
            "m_pendingConsultationLabel",
            "m_inConsultationLabel",
            "m_reviewCountLabel",
            "m_finishedTodayLabel",
            "rowsUpdated(const QJsonArray& rows)",
            "开始接诊",
            "接诊中",
            "检查完成待复诊"
        })) {
        return 3;
    }

    return 0;
}
