#include "client/pages/Pages.h"

namespace hospital::client {

OperationLogPage::OperationLogPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("操作日志", "记录新增、修改、删除、收费、退费等关键业务操作，满足系统留痕要求。", "operationLog", "list", apiClient, parent)
{
}

} // namespace hospital::client
