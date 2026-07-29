#include "server/ScheduleRuleEngine.h"

#include <QRegularExpression>

namespace hospital::server {
namespace {

int weekdayValue(const QString& text)
{
    if (text == "周一" || text == "星期一" || text == "礼拜一" || text == "一") return 1;
    if (text == "周二" || text == "星期二" || text == "礼拜二" || text == "二") return 2;
    if (text == "周三" || text == "星期三" || text == "礼拜三" || text == "三") return 4;
    if (text == "周四" || text == "星期四" || text == "礼拜四" || text == "四") return 8;
    if (text == "周五" || text == "星期五" || text == "礼拜五" || text == "五") return 16;
    if (text == "周六" || text == "星期六" || text == "礼拜六" || text == "六") return 32;
    if (text == "周日" || text == "周天" || text == "星期日" || text == "星期天" || text == "礼拜日" || text == "礼拜天" || text == "日" || text == "天") return 64;
    return 0;
}

int weekdayMaskForText(QString text)
{
    text.replace("，", ",");
    text.replace("、", ",");
    text.replace("；", ",");
    text.replace(";", ",");
    text.replace("和", ",");
    text.replace(" ", ",");

    int mask = 0;
    for (const QString& rawPart : text.split(',', Qt::SkipEmptyParts)) {
        const QString part = rawPart.trimmed();
        const int direct = weekdayValue(part);
        if (direct > 0) {
            mask |= direct;
            continue;
        }

        static const QRegularExpression compactExpression("(?:周|星期|礼拜)?([一二三四五六日天])");
        auto matches = compactExpression.globalMatch(part);
        while (matches.hasNext()) {
            mask |= weekdayValue(matches.next().captured(1));
        }
    }
    return mask;
}

QString ruleTypeForMode(const QString& mode)
{
    if (mode == "禁排" || mode == "不可排" || mode.compare("BLOCK", Qt::CaseInsensitive) == 0) {
        return "BLOCK";
    }
    if (mode == "可排" || mode.compare("ALLOW", Qt::CaseInsensitive) == 0) {
        return "ALLOW";
    }
    return {};
}

bool targetMatches(const ScheduleRule& rule, const ScheduleRuleMatchContext& context)
{
    const QString targetType = rule.targetType.toUpper();
    if (targetType == "ALL") {
        return true;
    }
    if (targetType == "DOCTOR") {
        return (rule.doctorId > 0 && rule.doctorId == context.doctorId)
            || (!rule.targetText.trimmed().isEmpty() && rule.targetText.trimmed() == context.doctorName);
    }
    if (targetType == "DEPARTMENT") {
        return (rule.departmentId > 0 && rule.departmentId == context.departmentId)
            || (!rule.targetText.trimmed().isEmpty() && rule.targetText.trimmed() == context.departmentName);
    }
    if (targetType == "TITLE") {
        return (!rule.title.trimmed().isEmpty() && rule.title.trimmed() == context.title)
            || (!rule.targetText.trimmed().isEmpty() && rule.targetText.trimmed() == context.title);
    }
    return false;
}

bool dateMatches(const ScheduleRule& rule, const QDate& workDate)
{
    const QString dateMode = rule.dateMode.toUpper();
    if (dateMode == "ALL") {
        return true;
    }
    if (!workDate.isValid()) {
        return false;
    }
    if (dateMode == "DATE") {
        return rule.startDate.isValid() && rule.startDate == workDate;
    }
    if (dateMode == "DATE_RANGE") {
        return rule.startDate.isValid()
            && rule.endDate.isValid()
            && workDate >= rule.startDate
            && workDate <= rule.endDate;
    }
    if (dateMode == "WEEKLY") {
        const int bit = 1 << (workDate.dayOfWeek() - 1);
        return (rule.weekdaysMask & bit) != 0;
    }
    return false;
}

QString ruleReason(const ScheduleRule& rule)
{
    if (!rule.reason.trimmed().isEmpty()) {
        return rule.reason.trimmed();
    }
    if (!rule.rawText.trimmed().isEmpty()) {
        return rule.rawText.trimmed();
    }
    return "长期排班规则";
}

} // namespace

ScheduleRuleParseResult ScheduleRuleEngine::parseFixedRuleText(const QString& text)
{
    QString normalized = text.trimmed();
    normalized.replace("：", "|");
    const QStringList parts = normalized.split('|', Qt::KeepEmptyParts);
    if (parts.size() < 3) {
        return {false, "固定格式应为：禁排|医生姓名|周一,周五|原因", {}, {}, {}, 0, {}, {}, {}};
    }

    ScheduleRuleParseResult result;
    result.ruleType = ruleTypeForMode(parts.at(0).trimmed());
    if (result.ruleType.isEmpty()) {
        result.message = "规则类型必须是 禁排 或 可排。";
        return result;
    }

    result.targetText = parts.at(1).trimmed();
    if (result.targetText.isEmpty()) {
        result.message = "规则目标不能为空。";
        return result;
    }

    const QString timeText = parts.at(2).trimmed();
    if (timeText.isEmpty() || timeText == "全部" || timeText == "每天") {
        result.dateMode = "ALL";
    } else if (timeText.contains('~')) {
        const QStringList range = timeText.split('~', Qt::SkipEmptyParts);
        if (range.size() != 2) {
            result.message = "日期范围应为 2026-06-10~2026-06-12。";
            return result;
        }
        result.startDate = QDate::fromString(range.at(0).trimmed(), "yyyy-MM-dd");
        result.endDate = QDate::fromString(range.at(1).trimmed(), "yyyy-MM-dd");
        if (!result.startDate.isValid() || !result.endDate.isValid() || result.startDate > result.endDate) {
            result.message = "日期范围无效。";
            return result;
        }
        result.dateMode = "DATE_RANGE";
    } else {
        const QDate date = QDate::fromString(timeText, "yyyy-MM-dd");
        if (date.isValid()) {
            result.startDate = date;
            result.dateMode = "DATE";
        } else {
            result.weekdaysMask = weekdayMaskForText(timeText);
            if (result.weekdaysMask == 0) {
                result.message = "时间必须是 周一,周五、2026-06-10、2026-06-10~2026-06-12 或 全部。";
                return result;
            }
            result.dateMode = "WEEKLY";
        }
    }

    if (parts.size() > 3) {
        QStringList reasonParts;
        for (int i = 3; i < parts.size(); ++i) {
            const QString reason = parts.at(i).trimmed();
            if (!reason.isEmpty()) {
                reasonParts.append(reason);
            }
        }
        result.reason = reasonParts.join("|");
    }

    result.success = true;
    return result;
}

ScheduleRuleValidationResult ScheduleRuleEngine::validate(const QVector<ScheduleRule>& rules,
                                                          const ScheduleRuleMatchContext& context)
{
    bool hasMatchingAllowRule = false;
    bool matchedAllowDate = false;

    for (const ScheduleRule& rule : rules) {
        if (!targetMatches(rule, context)) {
            continue;
        }

        const QString ruleType = rule.ruleType.toUpper();
        const bool matchesDate = dateMatches(rule, context.workDate);
        if (ruleType == "BLOCK" && matchesDate) {
            return {false, QString("%1 %2 命中禁排规则：%3")
                .arg(context.doctorName,
                     context.workDate.toString("yyyy-MM-dd"),
                     ruleReason(rule))};
        }

        if (ruleType == "ALLOW") {
            hasMatchingAllowRule = true;
            matchedAllowDate = matchedAllowDate || matchesDate;
        }
    }

    if (hasMatchingAllowRule && !matchedAllowDate) {
        return {false, QString("%1 %2 不在服务端只允许排班的日期内。")
            .arg(context.doctorName, context.workDate.toString("yyyy-MM-dd"))};
    }

    return {true, "OK"};
}

} // namespace hospital::server
