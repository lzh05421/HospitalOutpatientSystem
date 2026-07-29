#include "client/pages/Pages.h"

namespace hospital::client {

PatientRecordPage::PatientRecordPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("患者病历档案", "按患者关联展示挂号、接诊、处方和费用记录，便于回看完整就诊历史。", "patientRecord", "list", apiClient, parent)
{
}

} // namespace hospital::client
