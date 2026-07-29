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
    const std::string moduleHeader = readFile("server/include/server/modules/ModuleServices.h");
    if (!containsAll(moduleHeader, {
            "class PermissionAdminService",
            "explicit PermissionAdminService(DatabaseManager* database)",
            "common::Response handle(const common::Request& request) override"
        })) {
        return 1;
    }

    const std::string service = readFile("server/src/modules/PermissionAdminService.cpp");
    if (!containsAll(service, {
            "PermissionAdminService::handle",
            "request.action == \"users\"",
            "request.action == \"roles\"",
            "request.action == \"createUser\"",
            "request.action == \"resetPassword\"",
            "request.action == \"toggleUser\"",
            "request.action == \"saveRolePermissions\"",
            "sys_user",
            "sys_role",
            "sys_user_role",
            "sys_menu",
            "sys_role_menu",
            "upsertLegacyUser",
            "UPDATE users SET status",
            "UPDATE users SET password_hash",
            "QCryptographicHash",
            "\"123456\"",
            "transaction()",
            "commit()",
            "rollback()"
        })) {
        return 2;
    }

    const std::string serverCmake = readFile("server/CMakeLists.txt");
    if (serverCmake.find("src/modules/PermissionAdminService.cpp") == std::string::npos) {
        return 3;
    }

    const std::string main = readFile("server/src/main.cpp");
    if (!containsAll(main, {
            "PermissionAdminService permissionAdminService(&database)",
            "router.registerService(\"permissionAdmin\", &permissionAdminService)"
        })) {
        return 4;
    }

    const std::string authorization = readFile("server/src/AuthorizationService.cpp");
    if (!containsAll(authorization, {
            "permissionAdmin:users",
            "permissionAdmin:roles",
            "permissionAdmin:createUser",
            "permissionAdmin:resetPassword",
            "permissionAdmin:toggleUser",
            "permissionAdmin:saveRolePermissions",
            "operationLog:list"
        })) {
        return 5;
    }

    const std::string database = readFile("server/src/DatabaseManager.cpp");
    if (!containsAll(database, {
            "'permission_admin_users'",
            "'permission_admin_roles'",
            "'permissionAdmin:users'",
            "'permissionAdmin:roles'",
            "'operation_log_list'",
            "'operationLog:list'",
            "INSERT INTO sys_user",
            "UPDATE sys_user_role ur",
            "SET ur.is_primary = 0",
            "ON DUPLICATE KEY UPDATE is_primary = VALUES(is_primary)",
            "WHERE su.username = 'admin'",
            "INSERT IGNORE INTO sys_role_menu",
            "seedCatalogDoctors(m_database)",
            "syncLegacyUsersToRbac(m_database"
        })) {
        return 6;
    }

    const std::string pagesHeader = readFile("client/include/client/pages/Pages.h");
    if (pagesHeader.find("class PermissionAdminPage") == std::string::npos) {
        return 7;
    }

    const std::string page = readFile("client/src/pages/PermissionAdminPage.cpp");
    if (!containsAll(page, {
            "PermissionAdminPage::PermissionAdminPage",
            "\"permissionAdmin\"",
            "\"users\"",
            "\"roles\"",
            "\"createUser\"",
            "\"resetPassword\"",
            "\"toggleUser\"",
            "\"saveRolePermissions\"",
            "QTableWidget",
            "QListWidget",
            "Qt::CheckStateRole",
            "updatePermissionItemVisual",
            "已授权",
            "未授权",
            "itemChanged",
            "新增账号",
            "重置密码",
            "停用账号",
            "保存角色权限"
        })) {
        return 8;
    }

    const std::string apiClient = readFile("client/include/client/ApiClient.h")
        + readFile("client/src/ApiClient.cpp");
    if (!containsAll(apiClient, {
            "hasPermission",
            "m_permissions",
            "permissions"
        })) {
        return 11;
    }

    const std::string doctorPage = readFile("client/src/pages/DoctorManagementPage.cpp");
    if (!containsAll(doctorPage, {
            "hasPermission(\"doctor:create\")",
            "hasPermission(\"doctor:update\")",
            "hasPermission(\"doctor:delete\")",
            "setVisible"
        })) {
        return 12;
    }

    const std::string drugInventoryPage = readFile("client/src/pages/DrugInventoryPage.cpp");
    if (drugInventoryPage.find("药品入库已提交") != std::string::npos) {
        return 13;
    }

    const std::string clientCmake = readFile("client/CMakeLists.txt");
    if (clientCmake.find("src/pages/PermissionAdminPage.cpp") == std::string::npos) {
        return 9;
    }

    const std::string mainWindow = readFile("client/src/MainWindow.cpp");
    if (!containsAll(mainWindow, {
            "addIfAllowed(\"权限配置\", {\"ADMIN\"",
            "roles.contains(m_apiClient->roleCode())",
            "new PermissionAdminPage(m_apiClient, this)"
        })) {
        return 10;
    }

    return 0;
}
