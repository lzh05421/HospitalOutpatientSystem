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
    const std::string mainWindow = readFile("client/src/MainWindow.cpp");
    if (!containsAll(mainWindow, {
            "sidebarBrandPanel",
            "moduleStatusPill",
            "navSectionLabel",
            "OPD Workstation",
            "在线工作台",
            "QFrame#sidebarBrandPanel",
            "QLabel#moduleStatusPill",
            "QLabel#navSectionLabel"
        })) {
        return 1;
    }

    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(modulePage, {
            "pageHeaderPanel",
            "tableShell",
            "tableToolbar",
            "emptyStateLabel",
            "createTableToolbar",
            "暂无数据",
            "QFrame#pageHeaderPanel",
            "QFrame#tableShell",
            "QFrame#tableToolbar"
        })) {
        return 2;
    }

    return 0;
}
