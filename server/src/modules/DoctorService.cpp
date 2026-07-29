#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace hospital::server {
namespace {

QString sha256(const QString& text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

common::Response ensureDepartment(DatabaseManager* database, const QString& departmentName, qint64* departmentId)
{
    if (departmentName.trimmed().isEmpty()) {
        return {false, "科室不能为空。", {}};
    }

    QSqlQuery query(database->database());
    query.prepare("SELECT id FROM departments WHERE dept_name = :dept LIMIT 1");
    query.bindValue(":dept", departmentName.trimmed());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    if (query.next()) {
        *departmentId = query.value(0).toLongLong();
        query.finish();
        return {true, "OK", {}};
    }
    query.finish();

    query.prepare("INSERT INTO departments (dept_code, dept_name, location, status) "
                  "VALUES (:dept_code, :dept_name, '', 1)");
    query.bindValue(":dept_code", "DEP" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
    query.bindValue(":dept_name", departmentName.trimmed());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    *departmentId = query.lastInsertId().toLongLong();
    return {true, "新科室已保存。", {}};
}

common::Response createDoctor(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    const QString name = payload.value("医生姓名").toString().trimmed();
    const QString department = payload.value("所属科室").toString().trimmed();
    const QString title = payload.value("职称").toString().trimmed();
    const QString specialty = payload.value("擅长方向").toString().trimmed();
    const QString phone = payload.value("电话").toString().trimmed();
    const double fee = payload.value("挂号费").toVariant().toDouble();
    if (name.isEmpty() || department.isEmpty()) {
        return {false, "医生姓名和所属科室不能为空。", {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    qint64 departmentId = 0;
    auto deptResult = ensureDepartment(database, department, &departmentId);
    if (!deptResult.success) {
        db.rollback();
        return deptResult;
    }

    QSqlQuery query(db);
    query.prepare("SELECT doc.id, u.id FROM doctors doc "
                  "JOIN users u ON u.id = doc.user_id "
                  "WHERE u.real_name = :name LIMIT 1");
    query.bindValue(":name", name);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    if (query.next()) {
        const qint64 doctorId = query.value(0).toLongLong();
        const qint64 userId = query.value(1).toLongLong();
        query.finish();

        QSqlQuery updateUser(db);
        updateUser.prepare("UPDATE users SET phone = :phone, status = 1 WHERE id = :id");
        updateUser.bindValue(":phone", phone);
        updateUser.bindValue(":id", userId);
        if (!updateUser.exec()) {
            db.rollback();
            return {false, updateUser.lastError().text(), {}};
        }

        QSqlQuery updateDoctor(db);
        updateDoctor.prepare("UPDATE doctors SET department_id = :department_id, title = :title, "
                             "specialty = :specialty, registration_fee = :fee, status = 1 WHERE id = :id");
        updateDoctor.bindValue(":department_id", departmentId);
        updateDoctor.bindValue(":title", title);
        updateDoctor.bindValue(":specialty", specialty);
        updateDoctor.bindValue(":fee", fee);
        updateDoctor.bindValue(":id", doctorId);
        if (!updateDoctor.exec()) {
            db.rollback();
            return {false, updateDoctor.lastError().text(), {}};
        }

        db.commit();
        return {true, "医生已存在，已恢复并更新信息。", {}};
    }
    query.finish();

    QSqlQuery roleQuery(db);
    roleQuery.prepare("SELECT id FROM roles WHERE role_code = 'DOCTOR' LIMIT 1");
    if (!roleQuery.exec() || !roleQuery.next()) {
        db.rollback();
        return {false, "未找到 DOCTOR 角色，请检查数据库初始化。", {}};
    }
    const qint64 roleId = roleQuery.value(0).toLongLong();
    roleQuery.finish();

    QSqlQuery insertUser(db);
    insertUser.prepare("INSERT INTO users (username, password_hash, real_name, phone, role_id, status) "
                       "VALUES (:username, :password_hash, :real_name, :phone, :role_id, 1)");
    insertUser.bindValue(":username", "doctor" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
    insertUser.bindValue(":password_hash", sha256("123456"));
    insertUser.bindValue(":real_name", name);
    insertUser.bindValue(":phone", phone);
    insertUser.bindValue(":role_id", roleId);
    if (!insertUser.exec()) {
        db.rollback();
        return {false, insertUser.lastError().text(), {}};
    }
    const qint64 userId = insertUser.lastInsertId().toLongLong();

    QSqlQuery insertDoctor(db);
    insertDoctor.prepare("INSERT INTO doctors (user_id, department_id, title, specialty, registration_fee, status) "
                         "VALUES (:user_id, :department_id, :title, :specialty, :fee, 1)");
    insertDoctor.bindValue(":user_id", userId);
    insertDoctor.bindValue(":department_id", departmentId);
    insertDoctor.bindValue(":title", title);
    insertDoctor.bindValue(":specialty", specialty);
    insertDoctor.bindValue(":fee", fee);
    if (!insertDoctor.exec()) {
        db.rollback();
        return {false, insertDoctor.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "医生已新增，科室已同步保存。请到医生排班为该医生设置号源后，患者端即可预约。", {}};
}

common::Response updateDoctor(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("SELECT doc.id, u.id FROM doctors doc JOIN users u ON u.id = doc.user_id "
                  "WHERE u.real_name = :old_name LIMIT 1");
    query.bindValue(":old_name", payload.value("原医生姓名").toString(payload.value("医生姓名").toString()));
    if (!query.exec() || !query.next()) {
        db.rollback();
        return {false, "未找到要修改的医生。", {}};
    }
    const qint64 doctorId = query.value(0).toLongLong();
    const qint64 userId = query.value(1).toLongLong();
    query.finish();

    qint64 departmentId = 0;
    auto deptResult = ensureDepartment(database, payload.value("所属科室").toString(), &departmentId);
    if (!deptResult.success) {
        db.rollback();
        return deptResult;
    }

    query.prepare("UPDATE users SET real_name = :name, phone = :phone WHERE id = :id");
    query.bindValue(":name", payload.value("医生姓名").toString());
    query.bindValue(":phone", payload.value("电话").toString());
    query.bindValue(":id", userId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    query.prepare("UPDATE doctors SET department_id = :department_id, title = :title, specialty = :specialty, "
                  "registration_fee = :fee, status = :status WHERE id = :id");
    query.bindValue(":department_id", departmentId);
    query.bindValue(":title", payload.value("职称").toString());
    query.bindValue(":specialty", payload.value("擅长方向").toString());
    query.bindValue(":fee", payload.value("挂号费").toVariant().toDouble());
    const int status = payload.contains("状态") ? payload.value("状态").toVariant().toInt() : 1;
    query.bindValue(":status", status);
    query.bindValue(":id", doctorId);
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }
    db.commit();
    return {true, "医生信息已修改。", {}};
}

common::Response disableDoctor(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    auto db = database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    QSqlQuery query(db);
    query.prepare("UPDATE doctors doc JOIN users u ON u.id = doc.user_id "
                  "SET doc.status = 0, u.status = 0 WHERE u.real_name = :name");
    query.bindValue(":name", payload.value("医生姓名").toString());
    if (!query.exec()) {
        db.rollback();
        return {false, query.lastError().text(), {}};
    }

    QSqlQuery schedule(db);
    schedule.prepare("UPDATE doctor_schedules s "
                     "JOIN doctors doc ON doc.id = s.doctor_id "
                     "JOIN users u ON u.id = doc.user_id "
                     "SET s.status = 0, s.remain_quota = 0 "
                     "WHERE u.real_name = :name");
    schedule.bindValue(":name", payload.value("医生姓名").toString());
    if (!schedule.exec()) {
        db.rollback();
        return {false, schedule.lastError().text(), {}};
    }

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }
    return {true, "医生已停用。", {}};
}

} // namespace

DoctorService::DoctorService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response DoctorService::handle(const common::Request& request)
{
    if (request.action == "create") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().saveDoctor(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return createDoctor(m_database, request.payload);
    }
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updateDoctor(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updateDoctor(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().disableDoctor(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return disableDoctor(m_database, request.payload);
    }
    if (request.action != "list") {
        return {false, "Unsupported doctor action", {}};
    }

    return SqlJson::selectRows(m_database,
        "SELECT u.real_name AS '医生姓名', d.dept_name AS '所属科室', doc.title AS '职称', "
        "doc.specialty AS '擅长方向', doc.registration_fee AS '挂号费', "
        "u.phone AS '电话', doc.status AS '状态' "
        "FROM doctors doc "
        "JOIN users u ON u.id = doc.user_id "
        "JOIN departments d ON d.id = doc.department_id "
        "WHERE doc.status = 1 AND u.status = 1 "
        "ORDER BY d.dept_name, u.real_name LIMIT 300",
        {}, "doctors");
}

} // namespace hospital::server
