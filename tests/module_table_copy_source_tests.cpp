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
    const std::string header = readFile("client/include/client/pages/ModulePage.h");
    const std::string source = readFile("client/src/pages/ModulePage.cpp");
    const std::string combined = header + source;

    if (!containsAll(combined, {
            "QClipboard",
            "QShortcut",
            "QMenu",
            "setContextMenuPolicy(Qt::CustomContextMenu)",
            "QKeySequence::Copy",
            "showTableContextMenu",
            "copyCurrentCell",
            "copySelectedRegistrationNo",
            "复制当前单元格",
            "复制挂号单号",
            "QApplication::clipboard()->setText"
        })) {
        return 1;
    }

    if (!containsAll(source, {
            "const bool hasDoctorFilter",
            "const bool hasClinicTypeFilter",
            "if (group.isEmpty() && !hasDoctorFilter && !hasClinicTypeFilter)",
            "&& hasDoctorFilter",
            "&& hasClinicTypeFilter"
        })) {
        return 2;
    }

    return 0;
}
