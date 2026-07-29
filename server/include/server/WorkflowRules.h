#pragma once

#include <QString>
#include <QStringList>

namespace hospital { namespace server {

class WorkflowRules
{
public:
    static QString pendingPayment();
    static QString waiting();
    static QString called();
    static QString inConsultation();
    static QString checking();
    static QString checkDone();
    static QString finished();
    static QString cancelled();

    static QStringList validRegistrationStatuses();
    static bool isValidRegistrationStatus(const QString& status);
    static bool isWaitingQueueStatus(const QString& status);
    static bool isDoctorConsultationStatus(const QString& status);
    static bool canCall(const QString& status);
    static bool canStartConsultation(const QString& status);
    static bool canFinishConsultation(const QString& status);
    static bool canCancelRegistration(const QString& status);
    static QString statusAfterExaminationRequested();
    static QString statusAfterExaminationCompleted();
    static QString displayText(const QString& status);
};

}} // namespace hospital::server
