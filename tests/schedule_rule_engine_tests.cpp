#include "server/ScheduleRuleEngine.h"

#include <QtTest/QtTest>

using hospital::server::ScheduleRule;
using hospital::server::ScheduleRuleEngine;
using hospital::server::ScheduleRuleMatchContext;
using hospital::server::ScheduleRuleParseResult;
using hospital::server::ScheduleRuleValidationResult;

class ScheduleRuleEngineTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesFixedBlockRuleText();
    void blockRuleRejectsMatchingDoctorAndWeekday();
    void allowRuleRejectsNonAllowedWeekday();
};

void ScheduleRuleEngineTests::parsesFixedBlockRuleText()
{
    const ScheduleRuleParseResult result = ScheduleRuleEngine::parseFixedRuleText("禁排|刘晓民|周一,周五|外院");

    QVERIFY(result.success);
    QCOMPARE(result.ruleType, QString("BLOCK"));
    QCOMPARE(result.targetText, QString("刘晓民"));
    QCOMPARE(result.dateMode, QString("WEEKLY"));
    QCOMPARE(result.weekdaysMask, 17);
    QCOMPARE(result.reason, QString("外院"));
}

void ScheduleRuleEngineTests::blockRuleRejectsMatchingDoctorAndWeekday()
{
    ScheduleRule rule;
    rule.ruleType = "BLOCK";
    rule.targetType = "DOCTOR";
    rule.doctorId = 42;
    rule.dateMode = "WEEKLY";
    rule.weekdaysMask = 17;
    rule.reason = "外院";

    ScheduleRuleMatchContext context;
    context.doctorId = 42;
    context.departmentId = 7;
    context.doctorName = "刘晓民";
    context.departmentName = "呼吸内科诊室";
    context.title = "知名专家（三档）";
    context.workDate = QDate(2026, 6, 12);

    const ScheduleRuleValidationResult result = ScheduleRuleEngine::validate({rule}, context);

    QVERIFY(!result.allowed);
    QVERIFY(result.message.contains("刘晓民"));
    QVERIFY(result.message.contains("外院"));
}

void ScheduleRuleEngineTests::allowRuleRejectsNonAllowedWeekday()
{
    ScheduleRule rule;
    rule.ruleType = "ALLOW";
    rule.targetType = "DOCTOR";
    rule.doctorId = 42;
    rule.dateMode = "WEEKLY";
    rule.weekdaysMask = 17;
    rule.reason = "固定出诊";

    ScheduleRuleMatchContext context;
    context.doctorId = 42;
    context.departmentId = 7;
    context.doctorName = "刘晓民";
    context.departmentName = "呼吸内科诊室";
    context.title = "知名专家（三档）";
    context.workDate = QDate(2026, 6, 10);

    const ScheduleRuleValidationResult result = ScheduleRuleEngine::validate({rule}, context);

    QVERIFY(!result.allowed);
    QVERIFY(result.message.contains("只允许"));
}

QTEST_MAIN(ScheduleRuleEngineTests)
#include "schedule_rule_engine_tests.moc"
