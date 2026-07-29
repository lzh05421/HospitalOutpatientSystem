#include "client/pages/Pages.h"

namespace hospital::client {

RegistrationPage::RegistrationPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("挂号管理", "按科室、医生和排班号源完成挂号、退号和候诊状态维护。", "registration", "list", apiClient, parent, 5000)
{
}

} // namespace hospital::client
