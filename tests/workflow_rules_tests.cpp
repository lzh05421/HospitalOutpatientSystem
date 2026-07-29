#include "server/WorkflowRules.h"

#include <QtTest/QtTest>

using hospital::server::WorkflowRules;

class WorkflowRulesTests : public QObject
{
    Q_OBJECT

private slots:
    void pendingPaymentIsValidButNotQueueVisible();
    void waitingRowsAreVisibleOnlyInWaitingQueue();
    void doctorReadyRowsExcludeRawWaiting();
    void examinationCompletionReturnsPatientForFollowUp();
    void invalidRegistrationStatusIsRejected();
};

void WorkflowRulesTests::pendingPaymentIsValidButNotQueueVisible()
{
    QVERIFY(WorkflowRules::isValidRegistrationStatus("PENDING_PAYMENT"));
    QCOMPARE(WorkflowRules::displayText("PENDING_PAYMENT"), QString("待支付"));
    QVERIFY(!WorkflowRules::isWaitingQueueStatus("PENDING_PAYMENT"));
    QVERIFY(!WorkflowRules::isDoctorConsultationStatus("PENDING_PAYMENT"));
    QVERIFY(WorkflowRules::canCancelRegistration("PENDING_PAYMENT"));
}

void WorkflowRulesTests::waitingRowsAreVisibleOnlyInWaitingQueue()
{
    QVERIFY(WorkflowRules::isWaitingQueueStatus("WAITING"));
    QVERIFY(!WorkflowRules::isDoctorConsultationStatus("WAITING"));
}

void WorkflowRulesTests::doctorReadyRowsExcludeRawWaiting()
{
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("CALLED"));
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("IN_CONSULTATION"));
    QVERIFY(WorkflowRules::isDoctorConsultationStatus("CHECK_DONE"));
    QVERIFY(!WorkflowRules::isDoctorConsultationStatus("CHECKING"));
}

void WorkflowRulesTests::examinationCompletionReturnsPatientForFollowUp()
{
    QCOMPARE(WorkflowRules::statusAfterExaminationCompleted(), QString("CHECK_DONE"));
    QCOMPARE(WorkflowRules::displayText("CHECK_DONE"), QString("检查完成待复诊"));
}

void WorkflowRulesTests::invalidRegistrationStatusIsRejected()
{
    QVERIFY(WorkflowRules::isValidRegistrationStatus("WAITING"));
    QVERIFY(!WorkflowRules::isValidRegistrationStatus("随便填"));
}

QTEST_MAIN(WorkflowRulesTests)
#include "workflow_rules_tests.moc"
