#include "common/Protocol.h"
#include "server/DatabaseManager.h"
#include "server/modules/ModuleServices.h"

#include <QDate>
#include <QJsonArray>
#include <QtTest/QtTest>

using hospital::common::Request;
using hospital::common::Response;
using hospital::server::DatabaseManager;
using hospital::server::ScheduleService;

class ScheduleServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void rangeListReturnsSchedulesWithinRequestedDatesInDemoMode();
};

void ScheduleServiceTests::rangeListReturnsSchedulesWithinRequestedDatesInDemoMode()
{
    DatabaseManager database;
    ScheduleService service(&database);

    const QDate startDate = QDate::currentDate().addDays(-1);
    const QDate endDate = QDate::currentDate().addDays(7);
    const QDate seededDate = QDate::currentDate().addDays(3);

    Request saveRequest;
    saveRequest.module = "schedule";
    saveRequest.action = "save";
    saveRequest.payload["department"] = QString("事务测试诊室");
    saveRequest.payload["doctor"] = QString("范围接口测试医生");
    saveRequest.payload["title"] = QString("主治医师");
    saveRequest.payload["date"] = seededDate.toString("yyyy-MM-dd");
    saveRequest.payload["quota"] = 12;
    const Response saveResponse = service.handle(saveRequest);
    QVERIFY2(saveResponse.success, qPrintable(saveResponse.message));

    Request request;
    request.module = "schedule";
    request.action = "rangeList";
    request.payload["startDate"] = startDate.toString("yyyy-MM-dd");
    request.payload["endDate"] = endDate.toString("yyyy-MM-dd");

    const Response response = service.handle(request);

    QVERIFY2(response.success, qPrintable(response.message));
    QCOMPARE(response.data.value("action").toString(), QString("rangeList"));

    const QJsonArray rows = response.data.value("rows").toArray();
    QVERIFY(!rows.isEmpty());
    bool foundSeededSchedule = false;
    QString previousSortKey;
    for (const auto& item : rows) {
        const auto row = item.toObject();
        const QDate date = QDate::fromString(row.value("出诊日期").toString(), "yyyy-MM-dd");
        QVERIFY(date.isValid());
        QVERIFY(date >= startDate);
        QVERIFY(date <= endDate);
        const QString sortKey = row.value("出诊日期").toString()
            + "|" + row.value("科室").toString()
            + "|" + row.value("医生").toString();
        QVERIFY2(previousSortKey.isEmpty() || previousSortKey <= sortKey,
                 qPrintable(QString("排班范围列表未按开始日期升序返回：%1 > %2").arg(previousSortKey, sortKey)));
        previousSortKey = sortKey;
        if (row.value("医生").toString() == "范围接口测试医生"
            && date == seededDate) {
            foundSeededSchedule = true;
        }
    }
    QVERIFY(foundSeededSchedule);
}

QTEST_MAIN(ScheduleServiceTests)
#include "schedule_service_tests.moc"
