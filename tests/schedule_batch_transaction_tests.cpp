#include "common/Protocol.h"
#include "server/AppConfig.h"
#include "server/DatabaseManager.h"
#include "server/modules/ModuleServices.h"

#include <QDate>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest/QtTest>

using hospital::common::Request;
using hospital::common::Response;
using hospital::server::AppConfig;
using hospital::server::DatabaseManager;
using hospital::server::ScheduleService;

namespace {

QString envOrDefault(const char* name, const QString& defaultValue)
{
    const QString value = QString::fromLocal8Bit(qgetenv(name)).trimmed();
    return value.isEmpty() ? defaultValue : value;
}

bool execSql(QSqlDatabase& database, const QString& sql, const QVariantMap& binds = {})
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (auto it = binds.constBegin(); it != binds.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }
    const bool ok = query.exec();
    query.finish();
    return ok;
}

qint64 scalarId(QSqlDatabase& database, const QString& sql, const QVariantMap& binds = {})
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (auto it = binds.constBegin(); it != binds.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }
    if (!query.exec() || !query.next()) {
        query.finish();
        return 0;
    }
    const qint64 id = query.value(0).toLongLong();
    query.finish();
    return id;
}

int scheduleCount(QSqlDatabase& database, qint64 doctorId)
{
    return static_cast<int>(scalarId(database,
        "SELECT COUNT(*) FROM doctor_schedules WHERE doctor_id = :doctor_id",
        {{":doctor_id", doctorId}}));
}

void cleanupTestRows(QSqlDatabase& database,
                     qint64 doctorId,
                     qint64 userId,
                     qint64 departmentId,
                     qint64 roleId)
{
    if (doctorId > 0) {
        execSql(database, "DELETE FROM doctor_schedules WHERE doctor_id = :doctor_id", {{":doctor_id", doctorId}});
        execSql(database, "DELETE FROM doctors WHERE id = :doctor_id", {{":doctor_id", doctorId}});
    }
    if (userId > 0) {
        execSql(database, "DELETE FROM users WHERE id = :user_id", {{":user_id", userId}});
    }
    if (departmentId > 0) {
        execSql(database, "DELETE FROM departments WHERE id = :department_id", {{":department_id", departmentId}});
    }
    if (roleId > 0) {
        execSql(database, "DELETE FROM roles WHERE id = :role_id", {{":role_id", roleId}});
    }
}

struct TestRowsCleanup
{
    QSqlDatabase database;
    qint64 doctorId = 0;
    qint64 userId = 0;
    qint64 departmentId = 0;
    qint64 roleId = 0;
    bool active = true;

    ~TestRowsCleanup()
    {
        cleanup();
    }

    void cleanup()
    {
        if (!active || !database.isValid() || !database.isOpen()) {
            return;
        }
        cleanupTestRows(database, doctorId, userId, departmentId, roleId);
        active = false;
    }
};

} // namespace

class ScheduleBatchTransactionTests : public QObject
{
    Q_OBJECT

private slots:
    void batchSaveRollsBackAllRowsWhenFifthRowHasInvalidDate();
};

void ScheduleBatchTransactionTests::batchSaveRollsBackAllRowsWhenFifthRowHasInvalidDate()
{
    if (qEnvironmentVariable("HOSPITAL_RUN_MYSQL_INTEGRATION_TESTS") != "1") {
        QSKIP("Set HOSPITAL_RUN_MYSQL_INTEGRATION_TESTS=1 to run the MySQL transaction integration test.");
    }

    AppConfig config;
    config.databaseEnabled = true;
    config.databaseDriver = envOrDefault("HOSPITAL_TEST_DB_DRIVER", config.databaseDriver);
    config.databaseOdbcDriver = envOrDefault("HOSPITAL_TEST_DB_ODBC_DRIVER", config.databaseOdbcDriver);
    config.databaseHost = envOrDefault("HOSPITAL_TEST_DB_HOST", config.databaseHost);
    config.databasePort = envOrDefault("HOSPITAL_TEST_DB_PORT", QString::number(config.databasePort)).toInt();
    config.databaseName = envOrDefault("HOSPITAL_TEST_DB_NAME", config.databaseName);
    config.databaseUser = envOrDefault("HOSPITAL_TEST_DB_USER", config.databaseUser);
    config.databasePassword = envOrDefault("HOSPITAL_TEST_DB_PASSWORD", config.databasePassword);

    DatabaseManager database;
    QVERIFY2(database.open(config), qPrintable(database.lastError()));
    QSqlDatabase db = database.database();
    TestRowsCleanup cleanup{db};

    const QString suffix = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString roleCode = "TX_TEST_ROLE_" + suffix;
    const QString deptCode = "TX_TEST_DEPT_" + suffix;
    const QString username = "tx_test_user_" + suffix;
    const QString doctorName = QString::fromUtf8("事务测试医生") + suffix;
    const QString departmentName = QString::fromUtf8("事务测试诊室") + suffix;
    const QString passwordHash = QString(64, '0');

    qint64 roleId = 0;
    qint64 departmentId = 0;
    qint64 userId = 0;
    qint64 doctorId = 0;

    QVERIFY2(execSql(db,
        "INSERT INTO roles (role_code, role_name, description) VALUES (:code, '事务测试角色', 'batchSave rollback test')",
        {{":code", roleCode}}), qPrintable(db.lastError().text()));
    roleId = scalarId(db, "SELECT id FROM roles WHERE role_code = :code", {{":code", roleCode}});
    cleanup.roleId = roleId;
    QVERIFY(roleId > 0);

    QVERIFY2(execSql(db,
        "INSERT INTO departments (dept_code, dept_name, location, status) VALUES (:code, :name, '测试', 1)",
        {{":code", deptCode}, {":name", departmentName}}), qPrintable(db.lastError().text()));
    departmentId = scalarId(db, "SELECT id FROM departments WHERE dept_code = :code", {{":code", deptCode}});
    cleanup.departmentId = departmentId;
    QVERIFY(departmentId > 0);

    QVERIFY2(execSql(db,
        "INSERT INTO users (username, password_hash, real_name, role_id, status) "
        "VALUES (:username, :password_hash, :real_name, :role_id, 1)",
        {{":username", username}, {":password_hash", passwordHash}, {":real_name", doctorName}, {":role_id", roleId}}),
        qPrintable(db.lastError().text()));
    userId = scalarId(db, "SELECT id FROM users WHERE username = :username", {{":username", username}});
    cleanup.userId = userId;
    QVERIFY(userId > 0);

    QVERIFY2(execSql(db,
        "INSERT INTO doctors (user_id, department_id, title, specialty, registration_fee, status) "
        "VALUES (:user_id, :department_id, '主治医师', '事务测试', 0, 1)",
        {{":user_id", userId}, {":department_id", departmentId}}), qPrintable(db.lastError().text()));
    doctorId = scalarId(db, "SELECT id FROM doctors WHERE user_id = :user_id", {{":user_id", userId}});
    cleanup.doctorId = doctorId;
    QVERIFY(doctorId > 0);

    QVERIFY2(execSql(db,
        "DELETE FROM doctor_schedules WHERE doctor_id = :doctor_id",
        {{":doctor_id", doctorId}}), qPrintable(db.lastError().text()));
    const int before = scheduleCount(db, doctorId);
    QCOMPARE(before, 0);

    QJsonArray rows;
    for (int i = 0; i < 5; ++i) {
        QJsonObject row;
        row["doctor"] = doctorName;
        row["department"] = departmentName;
        row["title"] = QString::fromUtf8("主治医师");
        row["date"] = i == 4 ? QString("not-a-date") : QDate::currentDate().addDays(30 + i).toString("yyyy-MM-dd");
        row["quota"] = 30;
        rows.append(row);
    }

    Request request;
    request.module = "schedule";
    request.action = "batchSave";
    request.payload["rows"] = rows;

    ScheduleService service(&database);
    const Response response = service.handle(request);

    QVERIFY(!response.success);
    QVERIFY2(response.message.contains(QString::fromUtf8("已回滚所有排班")), qPrintable(response.message));
    QCOMPARE(response.data.value("rolledBack").toBool(), true);
    QCOMPARE(response.data.value("failedIndex").toInt(), 5);
    QCOMPARE(scheduleCount(db, doctorId), before);

    cleanup.cleanup();
}

QTEST_MAIN(ScheduleBatchTransactionTests)
#include "schedule_batch_transaction_tests.moc"
