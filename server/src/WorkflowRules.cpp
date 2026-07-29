#include "server/WorkflowRules.h"

namespace hospital::server {

QString WorkflowRules::pendingPayment() { return QStringLiteral("PENDING_PAYMENT"); }
QString WorkflowRules::waiting() { return QStringLiteral("WAITING"); }
QString WorkflowRules::called() { return QStringLiteral("CALLED"); }
QString WorkflowRules::inConsultation() { return QStringLiteral("IN_CONSULTATION"); }
QString WorkflowRules::checking() { return QStringLiteral("CHECKING"); }
QString WorkflowRules::checkDone() { return QStringLiteral("CHECK_DONE"); }
QString WorkflowRules::finished() { return QStringLiteral("FINISHED"); }
QString WorkflowRules::cancelled() { return QStringLiteral("CANCELLED"); }

QStringList WorkflowRules::validRegistrationStatuses()
{
    return {pendingPayment(), waiting(), called(), inConsultation(), checking(), checkDone(), finished(), cancelled()};
}

bool WorkflowRules::isValidRegistrationStatus(const QString& status)
{
    return validRegistrationStatuses().contains(status);
}

bool WorkflowRules::isWaitingQueueStatus(const QString& status)
{
    return status == waiting() || status == called() || status == checkDone();
}

bool WorkflowRules::isDoctorConsultationStatus(const QString& status)
{
    return status == called() || status == inConsultation() || status == checkDone();
}

bool WorkflowRules::canCall(const QString& status)
{
    return status == waiting() || status == checkDone();
}

bool WorkflowRules::canStartConsultation(const QString& status)
{
    return status == called() || status == checkDone();
}

bool WorkflowRules::canFinishConsultation(const QString& status)
{
    return status == inConsultation() || status == called() || status == checkDone();
}

bool WorkflowRules::canCancelRegistration(const QString& status)
{
    return status == pendingPayment() || status == waiting() || status == called();
}

QString WorkflowRules::statusAfterExaminationRequested()
{
    return checking();
}

QString WorkflowRules::statusAfterExaminationCompleted()
{
    return checkDone();
}

QString WorkflowRules::displayText(const QString& status)
{
    if (status == pendingPayment()) return QStringLiteral("待支付");
    if (status == waiting()) return QStringLiteral("待叫号");
    if (status == called()) return QStringLiteral("已叫号");
    if (status == inConsultation()) return QStringLiteral("接诊中");
    if (status == checking()) return QStringLiteral("检查中");
    if (status == checkDone()) return QStringLiteral("检查完成待复诊");
    if (status == finished()) return QStringLiteral("已接诊");
    if (status == cancelled()) return QStringLiteral("已取消");
    return status;
}

} // namespace hospital::server
