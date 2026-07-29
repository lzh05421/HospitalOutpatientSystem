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
    const std::string modulePage = readFile("client/src/pages/ModulePage.cpp");
    if (!containsAll(modulePage, {
            "applyWorkstationCellStyle",
            "urgentRowBrush",
            "statusTextBrush",
            "setObjectName(\"dangerButton\")",
            "setObjectName(\"warningButton\")",
            "急诊优先",
            "检查完成待复诊",
            "待叫号",
            "已叫号",
            "接诊中"
        })) {
        return 1;
    }

    const std::string mainWindow = readFile("client/src/MainWindow.cpp");
    if (!containsAll(mainWindow, {
            "QPushButton#dangerButton",
            "QPushButton#warningButton",
            "QTableWidget::item:hover",
            "QTableWidget::item:selected",
            "QComboBox::drop-down",
            "QHeaderView::section:hover"
        })) {
        return 2;
    }

    return 0;
}
