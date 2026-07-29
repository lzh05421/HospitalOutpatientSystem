#include "client/pages/Pages.h"

#include "client/ApiClient.h"

namespace hospital::client {

PatientPage::PatientPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage(apiClient && apiClient->roleCode() == "DOCTOR" ? "患者身份查询" : "患者管理",
                 apiClient && apiClient->roleCode() == "DOCTOR"
                     ? "医生用于查询已建档或已挂号患者，核对姓名、电话、身份证号、就诊次数和最近就诊记录。接诊和病历请到“医生接诊”和“患者病历档案”。"
                     : "通过患者编号、身份证/手机号登记和挂号就诊记录确认患者身份。",
                 "patient",
                 "list",
                 apiClient,
                 parent)
{
}

} // namespace hospital::client
