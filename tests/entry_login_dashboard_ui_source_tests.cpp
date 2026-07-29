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
    const std::string entryDialog = readFile("client/src/EntryDialog.cpp");
    if (!containsAll(entryDialog, {
            "heroPanel",
            "rolePanel",
            "statusPill",
            "entryAccent",
            "选择入口",
            "医疗工作站"
        })) {
        return 1;
    }

    const std::string loginDialog = readFile("client/src/LoginDialog.cpp");
    if (!containsAll(loginDialog, {
            "loginCard",
            "loginHero",
            "statusPill",
            "secureTip",
            "primaryButton",
            "正在验证账号"
        })) {
        return 2;
    }

    const std::string patientLoginDialog = readFile("client/src/PatientLoginDialog.cpp");
    if (!containsAll(patientLoginDialog, {
            "patientLoginHero",
            "patientLoginCard",
            "statusPill",
            "primaryButton",
            "新用户注册",
            "账号密码登录"
        })) {
        return 3;
    }

    const std::string dashboardPage = readFile("client/src/pages/DashboardPage.cpp");
    if (!containsAll(dashboardPage, {
            "dashboardHero",
            "dashboardMetricCard",
            "dashboardSectionCard",
            "chartSurface",
            "trendSummary",
            "今日门诊态势"
        })) {
        return 4;
    }

    return 0;
}
