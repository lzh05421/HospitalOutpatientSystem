#include "server/modules/AuthService.h"

#include "server/AuthorizationService.h"
#include "server/DatabaseManager.h"
#include "server/SessionManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

namespace hospital::server {
namespace {

QHash<QString, QJsonObject> demoUsers()
{
    return {
        {"admin", {{"userId", "1"}, {"realName", "系统管理员"}, {"roleCode", "ADMIN"}, {"roleName", "系统管理员"}}},
        {"director01", {{"userId", "2"}, {"realName", "内科门诊主任"}, {"roleCode", "DIRECTOR"}, {"roleName", "科主任"}}},
        {"reg01", {{"userId", "3"}, {"realName", "挂号员一号"}, {"roleCode", "REGISTRAR"}, {"roleName", "挂号员"}}},
        {"doctor01", {{"userId", "4"}, {"realName", "张明"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}},
        {"doctor02", {{"userId", "5"}, {"realName", "李华"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}},
        {"pharmacy01", {{"userId", "6"}, {"realName", "药房管理员"}, {"roleCode", "PHARMACIST"}, {"roleName", "药房人员"}}},
        {"cashier01", {{"userId", "7"}, {"realName", "收费员一号"}, {"roleCode", "CASHIER"}, {"roleName", "收费员"}}},
        {"doctor03", {{"userId", "8"}, {"realName", "周宁"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}},
        {"doctor04", {{"userId", "9"}, {"realName", "陈晓"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}},
        {"doctor05", {{"userId", "10"}, {"realName", "孙洁"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}},
        {"doctor06", {{"userId", "11"}, {"realName", "刘洋"}, {"roleCode", "DOCTOR"}, {"roleName", "医生"}}}
    };
}

QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values) {
        array.append(value);
    }
    return array;
}

bool queryUserFromNewSchema(const QSqlDatabase& database,
                            const QString& username,
                            const QString& sha256,
                            QString* userId,
                            QString* realName,
                            QString* roleCode,
                            QString* roleName)
{
    QSqlQuery query(database);
    query.prepare(
        "SELECT u.id, u.username, u.real_name, r.role_code, r.role_name "
        "FROM sys_user u "
        "JOIN sys_user_role ur ON ur.user_id = u.id "
        "JOIN sys_role r ON r.id = ur.role_id "
        "WHERE u.username = :username "
        "AND u.status = 1 "
        "AND u.password_hash = :sha256 "
        "AND r.status = 1 "
        "ORDER BY ur.is_primary DESC, r.id ASC "
        "LIMIT 1");
    query.bindValue(":username", username);
    query.bindValue(":sha256", sha256);

    if (!query.exec() || !query.next()) {
        return false;
    }

    if (userId) {
        *userId = QString::number(query.value(0).toLongLong());
    }
    if (realName) {
        *realName = query.value(2).toString();
    }
    if (roleCode) {
        *roleCode = query.value(3).toString();
    }
    if (roleName) {
        *roleName = query.value(4).toString();
    }
    return true;
}

QString saltedPasswordHash(const QString& password, const QString& salt)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        (salt + ":" + password).toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool isValidChineseIdCard(const QString& idCard)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d{17}[0-9Xx]$"));
    return pattern.match(idCard).hasMatch();
}

QJsonObject patientObjectFromQuery(const QSqlQuery& query,
                                   int idIndex,
                                   int nameIndex,
                                   int phoneIndex,
                                   int idCardIndex,
                                   int genderIndex,
                                   int relationshipIndex)
{
    QJsonObject patient;
    patient["patientId"] = QString::number(query.value(idIndex).toLongLong());
    patient["name"] = query.value(nameIndex).toString();
    patient["phone"] = query.value(phoneIndex).toString();
    patient["idCard"] = query.value(idCardIndex).toString();
    patient["gender"] = query.value(genderIndex).toString();
    patient["relationship"] = query.value(relationshipIndex).toString();
    return patient;
}

bool loadDefaultPatient(const QSqlDatabase& database,
                        const QString& patientUserId,
                        QJsonObject* patient)
{
    QSqlQuery query(database);
    query.prepare("SELECT id, name, phone, id_card, gender, relationship "
                  "FROM patients WHERE user_id = :user_id "
                  "ORDER BY CASE WHEN relationship = '本人' THEN 0 ELSE 1 END, id ASC LIMIT 1");
    query.bindValue(":user_id", patientUserId.toLongLong());
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (patient) {
        *patient = patientObjectFromQuery(query, 0, 1, 2, 3, 4, 5);
    }
    return true;
}

} // namespace

AuthService::AuthService(DatabaseManager* database, SessionManager* sessions)
    : m_database(database)
    , m_sessions(sessions)
{
}

common::Response AuthService::handle(const common::Request& request)
{
    if (request.action == "login") {
        return login(request);
    }
    if (request.action == "patientLogin") {
        return patientLogin(request);
    }
    if (request.action == "patientRegister") {
        return patientRegister(request);
    }
    if (request.action == "patientListMembers") {
        return patientListMembers(request);
    }
    if (request.action == "patientAddMember") {
        return patientAddMember(request);
    }

    return {false, "Unsupported auth action", {}};
}

common::Response AuthService::responseForSession(const QString& username,
                                                 const QString& userId,
                                                 const QString& realName,
                                                 const QString& roleCode,
                                                 const QString& roleName) const
{
    if (!m_sessions) {
        return {false, "服务端会话管理未初始化。", {}};
    }

    const auto profile = AuthorizationService::profileForUser(m_database, userId, username, roleCode);
    const auto session = m_sessions->createSession(userId,
                                                   username,
                                                   realName,
                                                   roleCode,
                                                   roleName,
                                                   profile.permissions,
                                                   profile.menus,
                                                   profile.dataScope,
                                                   profile.departmentIds,
                                                   profile.primaryDeptId,
                                                   profile.doctorId);
    QJsonObject data;
    data["token"] = session.token;
    data["userId"] = session.userId;
    data["username"] = session.username;
    data["realName"] = session.realName;
    data["roleCode"] = session.roleCode;
    data["roleName"] = session.roleName;
    data["permissions"] = toJsonArray(session.permissions);
    data["menus"] = toJsonArray(session.menus);
    data["dataScope"] = session.dataScope;
    data["deptIds"] = toJsonArray(session.departmentIds);
    data["primaryDeptId"] = session.primaryDeptId;
    data["doctorId"] = session.doctorId;
    data["loginTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return {true, "Login success", data};
}

common::Response AuthService::patientResponseForSession(const QString& patientUserId,
                                                        const QString& username,
                                                        const QString& name,
                                                        const QString& phone,
                                                        const QString& idCard,
                                                        const QString& selectedPatientId) const
{
    if (!m_sessions) {
        return {false, "服务端会话管理未初始化。", {}};
    }

    const auto session = m_sessions->createPatientSession(patientUserId, username, name, phone, idCard, selectedPatientId);
    QJsonObject data;
    data["token"] = session.token;
    data["userType"] = "PATIENT";
    data["patientUserId"] = session.patientUserId;
    data["patientId"] = session.patientId;
    data["userId"] = session.patientUserId;
    data["username"] = session.username;
    data["name"] = session.patientName;
    data["realName"] = session.patientName;
    data["phone"] = session.patientPhone;
    data["idCard"] = session.patientIdCard;
    data["roleCode"] = session.roleCode;
    data["roleName"] = session.roleName;
    data["loginTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return {true, "Patient login success", data};
}

common::Response AuthService::patientLogin(const common::Request& request)
{
    const QString username = request.payload.value("username").toString().trimmed();
    const QString password = request.payload.value("password").toString();
    if (username.isEmpty() || password.isEmpty()) {
        return {false, "请输入患者账号和密码。", {}};
    }

    if (!m_database->isEnabled()) {
        if ((username == "13800000001" || username == "P2024001") && password == "123456") {
            return patientResponseForSession("1", username, "张三", "13800000001", "110101199001011234", "1");
        }
        return {false, "患者账号或密码错误。演示患者：13800000001，密码 123456。", {}};
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery query(m_database->database());
    query.prepare("SELECT id, username, password_hash, password_salt "
                  "FROM patient_users WHERE username = :username LIMIT 1");
    query.bindValue(":username", username);
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    if (!query.next()) {
        return {false, "患者账号或密码错误。", {}};
    }

    const QString storedHash = query.value(2).toString();
    const QString passwordSalt = query.value(3).toString();
    if (storedHash.isEmpty() || passwordSalt.isEmpty()
        || saltedPasswordHash(password, passwordSalt) != storedHash) {
        return {false, "患者账号或密码错误。", {}};
    }

    const QString patientUserId = QString::number(query.value(0).toLongLong());
    const QString accountName = query.value(1).toString();
    query.finish();

    QJsonObject patient;
    if (!loadDefaultPatient(m_database->database(), patientUserId, &patient)) {
        return {false, "该账号尚未绑定就诊人，请先添加就诊人。", {}};
    }

    return patientResponseForSession(patientUserId,
                                     accountName,
                                     patient.value("name").toString(),
                                     patient.value("phone").toString(),
                                     patient.value("idCard").toString(),
                                     patient.value("patientId").toString());
}

common::Response AuthService::patientRegister(const common::Request& request)
{
    const QString phone = request.payload.value("phone").toString().trimmed();
    const QString username = request.payload.value("username").toString(phone).trimmed();
    const QString password = request.payload.value("password").toString();
    const QString name = request.payload.value("name").toString().trimmed();
    const QString idCard = request.payload.value("idCard").toString().trimmed();

    if (phone.isEmpty() || username.isEmpty() || password.isEmpty() || name.isEmpty() || idCard.isEmpty()) {
        return {false, "请完整填写姓名、手机号、身份证号和密码。", {}};
    }
    if (!QRegularExpression(QStringLiteral("^1\\d{10}$")).match(phone).hasMatch()) {
        return {false, "手机号格式不正确。", {}};
    }
    if (!isValidChineseIdCard(idCard)) {
        return {false, "身份证号格式不正确。", {}};
    }

    if (!m_database->isEnabled()) {
        return patientResponseForSession("99", username, name, phone, idCard);
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery exists(m_database->database());
    exists.prepare("SELECT id FROM patient_users WHERE username = :username LIMIT 1");
    exists.bindValue(":phone", phone);
    exists.bindValue(":username", username);
    if (!exists.exec()) {
        return {false, exists.lastError().text(), {}};
    }
    if (exists.next()) {
        return {false, "手机号已注册，请直接登录。", {}};
    }
    exists.finish();

    auto db = m_database->database();
    if (!db.transaction()) {
        return {false, db.lastError().text(), {}};
    }

    const QString passwordSalt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insertUser(db);
    insertUser.prepare("INSERT INTO patient_users (username, password_hash, password_salt) "
                       "VALUES (:username, :password_hash, :password_salt)");
    insertUser.bindValue(":username", username);
    insertUser.bindValue(":password_hash", saltedPasswordHash(password, passwordSalt));
    insertUser.bindValue(":password_salt", passwordSalt);
    if (!insertUser.exec()) {
        db.rollback();
        return {false, insertUser.lastError().text(), {}};
    }
    const QString patientUserId = QString::number(insertUser.lastInsertId().toLongLong());
    insertUser.finish();

    const QString patientNo = "P" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO patients "
                   "(patient_no, user_id, username, password_hash, password_salt, name, gender, id_card, phone, relationship, address) "
                   "VALUES (:patient_no, :user_id, :username, :password_hash, :password_salt, :name, '未知', :id_card, :phone, '本人', '')");
    insert.bindValue(":patient_no", patientNo);
    insert.bindValue(":user_id", patientUserId.toLongLong());
    insert.bindValue(":username", username);
    insert.bindValue(":password_hash", saltedPasswordHash(password, passwordSalt));
    insert.bindValue(":password_salt", passwordSalt);
    insert.bindValue(":name", name);
    insert.bindValue(":id_card", idCard);
    insert.bindValue(":phone", phone);
    if (!insert.exec()) {
        db.rollback();
        return {false, insert.lastError().text(), {}};
    }
    const QString selectedPatientId = QString::number(insert.lastInsertId().toLongLong());
    insert.finish();

    if (!db.commit()) {
        return {false, db.lastError().text(), {}};
    }

    return patientResponseForSession(patientUserId,
                                     username,
                                     name,
                                     phone,
                                     idCard,
                                     selectedPatientId);
}

common::Response AuthService::patientListMembers(const common::Request& request)
{
    const QString patientUserId = request.payload.value("__patientUserId").toVariant().toString().trimmed();
    if (patientUserId.isEmpty()) {
        return {false, "请先登录患者账号。", {}};
    }

    if (!m_database->isEnabled()) {
        QJsonArray rows{
            QJsonObject{{"patientId", "1"}, {"name", "张三"}, {"phone", "13800000001"}, {"idCard", "110101199001011234"}, {"gender", "男"}, {"relationship", "本人"}},
            QJsonObject{{"patientId", "2"}, {"name", "张小宝"}, {"phone", "13800000001"}, {"idCard", "110101201801011234"}, {"gender", "男"}, {"relationship", "子女"}}
        };
        return {true, "Demo patients", QJsonObject{{"rows", rows}, {"count", rows.size()}}};
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery query(m_database->database());
    query.prepare("SELECT id, name, phone, id_card, gender, relationship "
                  "FROM patients WHERE user_id = :user_id ORDER BY id ASC");
    query.bindValue(":user_id", patientUserId.toLongLong());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    QJsonArray rows;
    while (query.next()) {
        rows.append(patientObjectFromQuery(query, 0, 1, 2, 3, 4, 5));
    }
    return {true, "就诊人列表已加载。", QJsonObject{{"rows", rows}, {"count", rows.size()}}};
}

common::Response AuthService::patientAddMember(const common::Request& request)
{
    const QString patientUserId = request.payload.value("__patientUserId").toVariant().toString().trimmed();
    const QString name = request.payload.value("name").toString().trimmed();
    const QString phone = request.payload.value("phone").toString().trimmed();
    const QString idCard = request.payload.value("idCard").toString().trimmed();
    const QString gender = request.payload.value("gender").toString("未知").trimmed();
    const QString relationship = request.payload.value("relationship").toString("家属").trimmed();
    if (patientUserId.isEmpty()) {
        return {false, "请先登录患者账号。", {}};
    }
    if (name.isEmpty() || idCard.isEmpty()) {
        return {false, "请填写就诊人姓名和身份证号。", {}};
    }
    if (!isValidChineseIdCard(idCard)) {
        return {false, "身份证号格式不正确。", {}};
    }

    if (!m_database->isEnabled()) {
        QJsonObject patient{{"patientId", "2"}, {"name", name}, {"phone", phone}, {"idCard", idCard}, {"gender", gender}, {"relationship", relationship}};
        return {true, "就诊人已添加。", patient};
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    QSqlQuery exists(m_database->database());
    exists.prepare("SELECT id FROM patients WHERE user_id = :user_id AND id_card = :id_card LIMIT 1");
    exists.bindValue(":user_id", patientUserId.toLongLong());
    exists.bindValue(":id_card", idCard);
    if (!exists.exec()) {
        return {false, exists.lastError().text(), {}};
    }
    if (exists.next()) {
        return {false, "该就诊人已绑定到当前账号。", {}};
    }
    exists.finish();

    const QString patientNo = "P" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    QSqlQuery insert(m_database->database());
    insert.prepare("INSERT INTO patients (patient_no, user_id, name, gender, id_card, phone, relationship, address) "
                   "VALUES (:patient_no, :user_id, :name, :gender, :id_card, :phone, :relationship, '')");
    insert.bindValue(":patient_no", patientNo);
    insert.bindValue(":user_id", patientUserId.toLongLong());
    insert.bindValue(":name", name);
    insert.bindValue(":gender", gender.isEmpty() ? "未知" : gender);
    insert.bindValue(":id_card", idCard);
    insert.bindValue(":phone", phone);
    insert.bindValue(":relationship", relationship.isEmpty() ? "家属" : relationship);
    if (!insert.exec()) {
        return {false, insert.lastError().text(), {}};
    }

    QJsonObject patient{{"patientId", QString::number(insert.lastInsertId().toLongLong())},
                        {"name", name},
                        {"phone", phone},
                        {"idCard", idCard},
                        {"gender", gender},
                        {"relationship", relationship}};
    return {true, "就诊人已添加。", patient};
}

common::Response AuthService::login(const common::Request& request)
{
    const QString username = request.payload.value("username").toString();
    const QString password = request.payload.value("password").toString();

    if (username.isEmpty() || password.isEmpty()) {
        return {false, "请输入医院人员账号和密码。患者预约入口不需要账号。", {}};
    }

    if (!m_database->isEnabled()) {
        const auto users = demoUsers();
        if (!users.contains(username) || password != "123456") {
            return {false, "账号或密码错误。演示账号：admin、director01、reg01、doctor01、doctor02、doctor03、doctor04、doctor05、doctor06、pharmacy01、cashier01，密码均为 123456。", {}};
        }
        const auto user = users.value(username);
        return responseForSession(username,
                                  user.value("userId").toString(),
                                  user.value("realName").toString(),
                                  user.value("roleCode").toString(),
                                  user.value("roleName").toString());
    }

    if (!m_database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + m_database->lastError(), {}};
    }

    const QString sha256 = QString::fromLatin1(QCryptographicHash::hash(
        password.toUtf8(), QCryptographicHash::Sha256).toHex());

    QString userId;
    QString realName;
    QString roleCode;
    QString roleName;
    if (queryUserFromNewSchema(m_database->database(), username, sha256, &userId, &realName, &roleCode, &roleName)) {
        return responseForSession(username, userId, realName, roleCode, roleName);
    }

    QSqlQuery query(m_database->database());
    query.prepare(
        "SELECT u.id, u.username, u.real_name, r.role_code, r.role_name "
        "FROM users u "
        "JOIN roles r ON r.id = u.role_id "
        "WHERE u.username = :username "
        "AND u.status = 1 "
        "AND u.password_hash = :sha256");
    query.bindValue(":username", username);
    query.bindValue(":sha256", sha256);

    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    if (!query.next()) {
        return {false, "账号或密码错误。", {}};
    }

    const auto record = query.record();

    return responseForSession(query.value(record.indexOf("username")).toString(),
                              QString::number(query.value(record.indexOf("id")).toLongLong()),
                              query.value(record.indexOf("real_name")).toString(),
                              query.value(record.indexOf("role_code")).toString(),
                              query.value(record.indexOf("role_name")).toString());
}

} // namespace hospital::server
