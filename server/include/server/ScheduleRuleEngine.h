#pragma once

#include <QDate>
#include <QString>
#include <QVector>

namespace hospital { namespace server {

struct ScheduleRule
{
    QString ruleType;
    QString targetType;
    qint64 doctorId = 0;
    qint64 departmentId = 0;
    QString title;
    QString targetText;
    QString dateMode;
    int weekdaysMask = 0;
    QDate startDate;
    QDate endDate;
    QString reason;
    QString rawText;
};

struct ScheduleRuleMatchContext
{
    qint64 doctorId = 0;
    qint64 departmentId = 0;
    QString doctorName;
    QString departmentName;
    QString title;
    QDate workDate;
};

struct ScheduleRuleParseResult
{
    bool success = false;
    QString message;
    QString ruleType;
    QString targetText;
    QString dateMode;
    int weekdaysMask = 0;
    QDate startDate;
    QDate endDate;
    QString reason;
};

struct ScheduleRuleValidationResult
{
    bool allowed = true;
    QString message;
};

class ScheduleRuleEngine
{
public:
    static ScheduleRuleParseResult parseFixedRuleText(const QString& text);
    static ScheduleRuleValidationResult validate(const QVector<ScheduleRule>& rules,
                                                 const ScheduleRuleMatchContext& context);
};

}} // namespace hospital::server
