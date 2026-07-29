#include "server/DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace hospital::server {
namespace {

QString odbcConnectionStringWithConfiguredDriver(const AppConfig& config)
{
    return QString("DRIVER={%1};"
                   "SERVER=%2;"
                   "PORT=%3;"
                   "DATABASE=%4;"
                   "UID=%5;"
                   "PWD=%6;"
                   "NO_SSPS=1;"
                   "OPTION=3;")
        .arg(config.databaseOdbcDriver,
             config.databaseHost,
             QString::number(config.databasePort),
             config.databaseName,
             config.databaseUser,
             config.databasePassword);
}

QString driverHelp(const QString& driver)
{
    if (driver == "QODBC") {
        return "；当前使用 QODBC，请确认已安装 64 位 MySQL Connector/ODBC，并且 config/server.example.ini 里的 odbcDriver 名称和系统驱动名称完全一致";
    }

    if (driver == "QMYSQL") {
        return "；当前使用 QMYSQL，但你的 Qt sqldrivers 目录需要 qsqlmysql.dll";
    }

    return {};
}

void execIgnoreError(QSqlDatabase& database, const QString& sql)
{
    QSqlQuery query(database);
    query.exec(sql);
    query.finish();
}

struct CatalogDoctorSeed
{
    const char* category;
    const char* specialty;
    const char* clinic;
    const char* username;
    const char* doctor;
    const char* title;
    double fee;
    const char* phone;
};

const CatalogDoctorSeed catalogDoctorSeeds[] = {
    {"内科门诊", "心血管内科", "心血管内科诊室", "catalog_doctor001", "郑凯", "副主任医师", 20.00, "13920000001"},
    {"内科门诊", "心血管内科", "高血压门诊", "catalog_doctor002", "王立群", "主任医师", 25.00, "13920000002"},
    {"内科门诊", "心血管内科", "冠心病门诊", "catalog_doctor003", "冯若楠", "知名专家（三档）", 30.00, "13920000003"},
    {"内科门诊", "血液内科", "血液内科诊室", "catalog_doctor004", "韩亦辰", "知名专家（四档）", 50.00, "13920000004"},
    {"内科门诊", "血液内科", "儿童血液诊室", "catalog_doctor005", "许清源", "副主任医师", 20.00, "13920000005"},
    {"内科门诊", "血液内科", "贫血门诊", "catalog_doctor006", "沈嘉禾", "主任医师", 25.00, "13920000006"},
    {"内科门诊", "血液内科", "骨髓瘤门诊", "catalog_doctor007", "唐雨薇", "知名专家（三档）", 30.00, "13920000007"},
    {"内科门诊", "肾内科", "肾内科诊室", "catalog_doctor008", "曹明远", "知名专家（四档）", 50.00, "13920000008"},
    {"内科门诊", "肾内科", "慢性肾病门诊", "catalog_doctor009", "梁思远", "副主任医师", 20.00, "13920000009"},
    {"内科门诊", "肾内科", "血液透析门诊", "catalog_doctor010", "杜若溪", "主任医师", 25.00, "13920000010"},
    {"内科门诊", "呼吸内科", "呼吸内科诊室", "catalog_doctor011", "程浩然", "知名专家（三档）", 30.00, "13920000011"},
    {"内科门诊", "呼吸内科", "哮喘门诊", "catalog_doctor012", "叶安琪", "知名专家（四档）", 50.00, "13920000012"},
    {"内科门诊", "呼吸内科", "肺部感染门诊", "catalog_doctor013", "薛景行", "副主任医师", 20.00, "13920000013"},
    {"内科门诊", "消化内科", "消化内科诊室", "catalog_doctor014", "顾晓曼", "主任医师", 25.00, "13920000014"},
    {"内科门诊", "消化内科", "胃肠门诊", "catalog_doctor015", "罗云舟", "知名专家（三档）", 30.00, "13920000015"},
    {"内科门诊", "消化内科", "肝病门诊", "catalog_doctor016", "林知远", "知名专家（四档）", 50.00, "13920000016"},
    {"内科门诊", "内分泌科", "内分泌科诊室", "catalog_doctor017", "马思齐", "副主任医师", 20.00, "13920000017"},
    {"内科门诊", "内分泌科", "糖尿病门诊", "catalog_doctor018", "高子昂", "主任医师", 25.00, "13920000018"},
    {"内科门诊", "内分泌科", "甲状腺门诊", "catalog_doctor019", "宋雅宁", "知名专家（三档）", 30.00, "13920000019"},
    {"内科门诊", "神经内科", "神经内科诊室", "catalog_doctor020", "何沐阳", "知名专家（四档）", 50.00, "13920000020"},
    {"内科门诊", "神经内科", "头痛门诊", "catalog_doctor021", "魏舒然", "副主任医师", 20.00, "13920000021"},
    {"内科门诊", "神经内科", "脑卒中随访门诊", "catalog_doctor022", "潘嘉宁", "主任医师", 25.00, "13920000022"},
    {"内科门诊", "风湿免疫科", "风湿免疫科诊室", "catalog_doctor023", "方俊逸", "知名专家（三档）", 30.00, "13920000023"},
    {"内科门诊", "风湿免疫科", "类风湿门诊", "catalog_doctor024", "袁静姝", "知名专家（四档）", 50.00, "13920000024"},
    {"内科门诊", "风湿免疫科", "痛风门诊", "catalog_doctor025", "邹亦凡", "副主任医师", 20.00, "13920000025"},
    {"内科门诊", "老年医学科", "老年医学科诊室", "catalog_doctor026", "邵佳怡", "主任医师", 25.00, "13920000026"},
    {"内科门诊", "老年医学科", "老年慢病门诊", "catalog_doctor027", "钱思源", "知名专家（三档）", 30.00, "13920000027"},
    {"内科门诊", "老年医学科", "综合评估门诊", "catalog_doctor028", "贺明轩", "知名专家（四档）", 50.00, "13920000028"},
    {"外科门诊", "普外科", "普外科诊室", "catalog_doctor029", "孟雨桐", "副主任医师", 20.00, "13920000029"},
    {"外科门诊", "普外科", "胃肠外科门诊", "catalog_doctor030", "戴清和", "主任医师", 25.00, "13920000030"},
    {"外科门诊", "普外科", "肝胆外科门诊", "catalog_doctor031", "陆子涵", "知名专家（三档）", 30.00, "13920000031"},
    {"外科门诊", "骨科", "骨科诊室", "catalog_doctor032", "崔明朗", "知名专家（四档）", 50.00, "13920000032"},
    {"外科门诊", "骨科", "关节门诊", "catalog_doctor033", "夏以宁", "副主任医师", 20.00, "13920000033"},
    {"外科门诊", "骨科", "脊柱门诊", "catalog_doctor034", "钟景澄", "主任医师", 25.00, "13920000034"},
    {"外科门诊", "泌尿外科", "泌尿外科诊室", "catalog_doctor035", "姜若谷", "知名专家（三档）", 30.00, "13920000035"},
    {"外科门诊", "泌尿外科", "结石门诊", "catalog_doctor036", "白承泽", "知名专家（四档）", 50.00, "13920000036"},
    {"外科门诊", "泌尿外科", "前列腺门诊", "catalog_doctor037", "田梓涵", "副主任医师", 20.00, "13920000037"},
    {"外科门诊", "神经外科", "神经外科诊室", "catalog_doctor038", "孔令仪", "主任医师", 25.00, "13920000038"},
    {"外科门诊", "神经外科", "颅脑外伤门诊", "catalog_doctor039", "严子墨", "知名专家（三档）", 30.00, "13920000039"},
    {"外科门诊", "胸外科", "胸外科诊室", "catalog_doctor040", "任清妍", "知名专家（四档）", 50.00, "13920000040"},
    {"外科门诊", "胸外科", "肺结节门诊", "catalog_doctor041", "施予安", "副主任医师", 20.00, "13920000041"},
    {"儿科门诊", "儿科普通", "儿科普通诊室", "catalog_doctor042", "石文博", "主任医师", 25.00, "13920000042"},
    {"儿科门诊", "儿科普通", "儿童发热门诊", "catalog_doctor043", "范若晨", "知名专家（三档）", 30.00, "13920000043"},
    {"儿科门诊", "儿童血液", "儿童血液专病门诊", "catalog_doctor044", "乔思淼", "知名专家（四档）", 50.00, "13920000044"},
    {"儿科门诊", "儿童血液", "儿童贫血门诊", "catalog_doctor045", "邱逸凡", "副主任医师", 20.00, "13920000045"},
    {"儿科门诊", "儿童呼吸", "儿童呼吸诊室", "catalog_doctor046", "洪嘉树", "主任医师", 25.00, "13920000046"},
    {"儿科门诊", "儿童呼吸", "儿童哮喘门诊", "catalog_doctor047", "赖雨晴", "知名专家（三档）", 30.00, "13920000047"},
    {"儿科门诊", "儿童消化", "儿童消化诊室", "catalog_doctor048", "秦明哲", "知名专家（四档）", 50.00, "13920000048"},
    {"儿科门诊", "儿童消化", "儿童腹痛门诊", "catalog_doctor049", "倪语嫣", "副主任医师", 20.00, "13920000049"},
    {"妇产科门诊", "妇科", "妇科诊室", "catalog_doctor050", "汤书航", "主任医师", 25.00, "13920000050"},
    {"妇产科门诊", "妇科", "宫颈疾病门诊", "catalog_doctor051", "尹静远", "知名专家（三档）", 30.00, "13920000051"},
    {"妇产科门诊", "妇科", "月经病门诊", "catalog_doctor052", "常安然", "知名专家（四档）", 50.00, "13920000052"},
    {"妇产科门诊", "产科", "产科诊室", "catalog_doctor053", "黎昊天", "副主任医师", 20.00, "13920000053"},
    {"妇产科门诊", "产科", "孕期保健门诊", "catalog_doctor054", "莫清秋", "主任医师", 25.00, "13920000054"},
    {"妇产科门诊", "产科", "高危妊娠门诊", "catalog_doctor055", "傅子瑜", "知名专家（三档）", 30.00, "13920000055"},
    {"眼耳鼻喉门诊", "眼科", "眼科诊室", "catalog_doctor056", "万嘉懿", "知名专家（四档）", 50.00, "13920000056"},
    {"眼耳鼻喉门诊", "眼科", "视光门诊", "catalog_doctor057", "江明玥", "副主任医师", 20.00, "13920000057"},
    {"眼耳鼻喉门诊", "眼科", "白内障门诊", "catalog_doctor058", "段景然", "主任医师", 25.00, "13920000058"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "耳鼻喉科诊室", "catalog_doctor059", "梅若曦", "知名专家（三档）", 30.00, "13920000059"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "鼻炎门诊", "catalog_doctor060", "卢泽宇", "知名专家（四档）", 50.00, "13920000060"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "咽喉门诊", "catalog_doctor061", "郝星河", "副主任医师", 20.00, "13920000061"},
    {"口腔科门诊", "口腔内科", "口腔内科诊室", "catalog_doctor062", "毕语堂", "主任医师", 25.00, "13920000062"},
    {"口腔科门诊", "口腔内科", "牙体牙髓门诊", "catalog_doctor063", "康明睿", "知名专家（三档）", 30.00, "13920000063"},
    {"口腔科门诊", "口腔修复科", "口腔修复诊室", "catalog_doctor064", "毛思远", "知名专家（四档）", 50.00, "13920000064"},
    {"口腔科门诊", "口腔修复科", "种植修复门诊", "catalog_doctor065", "文嘉禾", "副主任医师", 20.00, "13920000065"},
    {"皮肤科门诊", "皮肤科", "皮肤科诊室", "catalog_doctor066", "葛雨辰", "主任医师", 25.00, "13920000066"},
    {"皮肤科门诊", "皮肤科", "湿疹门诊", "catalog_doctor067", "詹知夏", "知名专家（三档）", 30.00, "13920000067"},
    {"皮肤科门诊", "皮肤科", "痤疮门诊", "catalog_doctor068", "樊若云", "知名专家（四档）", 50.00, "13920000068"},
    {"中医科门诊", "中医内科", "中医内科诊室", "catalog_doctor069", "纪明扬", "副主任医师", 20.00, "13920000069"},
    {"中医科门诊", "中医内科", "失眠调理门诊", "catalog_doctor070", "温清越", "主任医师", 25.00, "13920000070"},
    {"中医科门诊", "中医内科", "针灸门诊", "catalog_doctor071", "龙启航", "知名专家（三档）", 30.00, "13920000071"},
    {"中医科门诊", "中医妇科", "中医妇科诊室", "catalog_doctor072", "向思衡", "知名专家（四档）", 50.00, "13920000072"},
    {"中医科门诊", "中医妇科", "月经调理门诊", "catalog_doctor073", "苗若琳", "副主任医师", 20.00, "13920000073"},
    {"中医科门诊", "康复理疗科", "康复理疗诊室", "catalog_doctor074", "申明远", "主任医师", 25.00, "13920000074"},
    {"中医科门诊", "康复理疗科", "颈肩腰腿痛门诊", "catalog_doctor075", "欧阳静安", "知名专家（三档）", 30.00, "13920000075"}
};

qint64 departmentIdByName(QSqlDatabase& database, const QString& name)
{
    QSqlQuery query(database);
    query.prepare("SELECT id FROM departments WHERE dept_name = :name AND status = 1 LIMIT 1");
    query.bindValue(":name", name);
    if (query.exec() && query.next()) {
        const qint64 id = query.value(0).toLongLong();
        query.finish();
        return id;
    }
    query.finish();
    return 0;
}

qint64 ensureCatalogDepartment(QSqlDatabase& database,
                               const QString& name,
                               const QString& location,
                               const QString& prefix,
                               int index)
{
    const qint64 existingId = departmentIdByName(database, name);
    if (existingId > 0) {
        return existingId;
    }

    QSqlQuery insert(database);
    insert.prepare("INSERT INTO departments (dept_code, dept_name, location, status) "
                   "VALUES (:code, :name, :location, 1) "
                   "ON DUPLICATE KEY UPDATE status = 1");
    insert.bindValue(":code", prefix + QString::number(index).rightJustified(3, '0'));
    insert.bindValue(":name", name);
    insert.bindValue(":location", location);
    if (!insert.exec()) {
        insert.finish();
        return 0;
    }
    insert.finish();
    return departmentIdByName(database, name);
}

qint64 ensureDoctorRole(QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.prepare("SELECT id FROM roles WHERE role_code = 'DOCTOR' LIMIT 1");
    if (query.exec() && query.next()) {
        const qint64 roleId = query.value(0).toLongLong();
        query.finish();
        return roleId;
    }
    query.finish();

    query.prepare("INSERT INTO roles (role_code, role_name, description) "
                  "VALUES ('DOCTOR', '医生', '接诊、诊断和开处方') "
                  "ON DUPLICATE KEY UPDATE role_name = VALUES(role_name)");
    if (!query.exec()) {
        query.finish();
        return 0;
    }
    query.finish();

    query.prepare("SELECT id FROM roles WHERE role_code = 'DOCTOR' LIMIT 1");
    if (query.exec() && query.next()) {
        const qint64 roleId = query.value(0).toLongLong();
        query.finish();
        return roleId;
    }
    query.finish();
    return 0;
}

qint64 ensureCatalogUser(QSqlDatabase& database, const CatalogDoctorSeed& seed, qint64 roleId)
{
    static const char* defaultPasswordHash =
        "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92";

    QSqlQuery upsert(database);
    upsert.prepare("INSERT INTO users (username, password_hash, real_name, phone, role_id, status) "
                   "VALUES (:username, :password_hash, :real_name, :phone, :role_id, 1) "
                   "ON DUPLICATE KEY UPDATE real_name = VALUES(real_name), "
                   "phone = VALUES(phone), role_id = VALUES(role_id), status = 1");
    upsert.bindValue(":username", QString::fromUtf8(seed.username));
    upsert.bindValue(":password_hash", QString::fromLatin1(defaultPasswordHash));
    upsert.bindValue(":real_name", QString::fromUtf8(seed.doctor));
    upsert.bindValue(":phone", QString::fromUtf8(seed.phone));
    upsert.bindValue(":role_id", roleId);
    if (!upsert.exec()) {
        upsert.finish();
        return 0;
    }
    upsert.finish();

    QSqlQuery query(database);
    query.prepare("SELECT id FROM users WHERE username = :username LIMIT 1");
    query.bindValue(":username", QString::fromUtf8(seed.username));
    if (query.exec() && query.next()) {
        const qint64 userId = query.value(0).toLongLong();
        query.finish();
        return userId;
    }
    query.finish();
    return 0;
}

void ensureCatalogDoctor(QSqlDatabase& database, const CatalogDoctorSeed& seed, int index, qint64 roleId)
{
    const QString category = QString::fromUtf8(seed.category);
    const QString specialty = QString::fromUtf8(seed.specialty);
    const QString clinic = QString::fromUtf8(seed.clinic);

    ensureCatalogDepartment(database, category, "门诊楼", "CATALOG_CAT_", index);
    ensureCatalogDepartment(database, specialty, category, "CATALOG_SPC_", index);
    const qint64 clinicId = ensureCatalogDepartment(database, clinic, category + "-" + specialty, "CATALOG_CLN_", index);
    const qint64 userId = ensureCatalogUser(database, seed, roleId);
    if (clinicId <= 0 || userId <= 0) {
        return;
    }

    QSqlQuery upsert(database);
    upsert.prepare("INSERT INTO doctors (user_id, department_id, title, specialty, registration_fee, status) "
                   "VALUES (:user_id, :department_id, :title, :specialty, :fee, 1) "
                   "ON DUPLICATE KEY UPDATE department_id = VALUES(department_id), "
                   "title = VALUES(title), specialty = VALUES(specialty), "
                   "registration_fee = VALUES(registration_fee), status = 1");
    upsert.bindValue(":user_id", userId);
    upsert.bindValue(":department_id", clinicId);
    upsert.bindValue(":title", QString::fromUtf8(seed.title));
    upsert.bindValue(":specialty", specialty + "常见病、多发病及" + clinic + "专病诊疗");
    upsert.bindValue(":fee", seed.fee);
    upsert.exec();
    upsert.finish();
}

void seedCatalogDoctors(QSqlDatabase& database)
{
    const qint64 roleId = ensureDoctorRole(database);
    if (roleId <= 0) {
        return;
    }

    for (int i = 0; i < static_cast<int>(std::size(catalogDoctorSeeds)); ++i) {
        ensureCatalogDoctor(database, catalogDoctorSeeds[i], i + 1, roleId);
    }
}

void syncLegacyUsersToRbac(QSqlDatabase& database, const QString& defaultPasswordHash)
{
    QSqlQuery syncUsers(database);
    syncUsers.prepare(
        "INSERT INTO sys_user (username, password_hash, real_name, phone, status) "
        "SELECT u.username, COALESCE(NULLIF(u.password_hash, ''), :password_hash), u.real_name, u.phone, u.status "
        "FROM users u WHERE u.username IS NOT NULL AND u.username <> '' "
        "ON DUPLICATE KEY UPDATE real_name = VALUES(real_name), phone = VALUES(phone), status = VALUES(status)");
    syncUsers.bindValue(":password_hash", defaultPasswordHash);
    syncUsers.exec();
    syncUsers.finish();

    QSqlQuery clearPrimaryRoles(database);
    clearPrimaryRoles.exec(
        "UPDATE sys_user_role ur "
        "JOIN sys_user su ON su.id = ur.user_id "
        "JOIN users u ON u.username = su.username "
        "SET ur.is_primary = 0");
    clearPrimaryRoles.finish();

    QSqlQuery syncRoles(database);
    syncRoles.exec(
        "INSERT INTO sys_user_role (user_id, role_id, is_primary) "
        "SELECT su.id, sr.id, 1 "
        "FROM sys_user su "
        "JOIN users u ON u.username = su.username "
        "JOIN roles r ON r.id = u.role_id "
        "JOIN sys_role sr ON sr.role_code = r.role_code "
        "ON DUPLICATE KEY UPDATE is_primary = VALUES(is_primary)");
    syncRoles.finish();

    QSqlQuery ensureAdminRole(database);
    ensureAdminRole.exec(
        "INSERT INTO sys_user_role (user_id, role_id, is_primary) "
        "SELECT su.id, sr.id, 1 "
        "FROM sys_user su "
        "JOIN sys_role sr ON sr.role_code = 'ADMIN' "
        "WHERE su.username = 'admin' "
        "ON DUPLICATE KEY UPDATE is_primary = VALUES(is_primary)");
    ensureAdminRole.finish();
}

} // namespace

bool DatabaseManager::open(const AppConfig& config)
{
    m_config = config;
    m_enabled = config.databaseEnabled;
    if (!m_enabled) {
        return true;
    }

    m_database = QSqlDatabase::addDatabase(m_config.databaseDriver);
    if (m_config.databaseDriver == "QODBC") {
        m_database.setDatabaseName(odbcConnectionStringWithConfiguredDriver(m_config));
    } else {
        m_database.setHostName(m_config.databaseHost);
        m_database.setPort(m_config.databasePort);
        m_database.setDatabaseName(m_config.databaseName);
        m_database.setUserName(m_config.databaseUser);
        m_database.setPassword(m_config.databasePassword);
        m_database.setConnectOptions("MYSQL_OPT_RECONNECT=1");
    }

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text()
            + "；当前 Qt SQL 驱动：" + QSqlDatabase::drivers().join(", ")
            + driverHelp(m_config.databaseDriver);
        return false;
    }

    ensureCompatibilitySchema();
    return true;
}

bool DatabaseManager::ensureOpen()
{
    if (!m_enabled) {
        return true;
    }

    if (m_database.isValid() && m_database.isOpen()) {
        return true;
    }

    if (!m_database.isValid()) {
        m_database = QSqlDatabase::addDatabase(m_config.databaseDriver);
        if (m_config.databaseDriver == "QODBC") {
            m_database.setDatabaseName(odbcConnectionStringWithConfiguredDriver(m_config));
        } else {
            m_database.setHostName(m_config.databaseHost);
            m_database.setPort(m_config.databasePort);
            m_database.setDatabaseName(m_config.databaseName);
            m_database.setUserName(m_config.databaseUser);
            m_database.setPassword(m_config.databasePassword);
            m_database.setConnectOptions("MYSQL_OPT_RECONNECT=1");
        }
    }

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text()
            + "；当前 Qt SQL 驱动：" + QSqlDatabase::drivers().join(", ")
            + driverHelp(m_config.databaseDriver);
        return false;
    }

    ensureCompatibilitySchema();
    m_lastError.clear();
    return true;
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}

bool DatabaseManager::isEnabled() const
{
    return m_enabled;
}

bool DatabaseManager::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

void DatabaseManager::ensureCompatibilitySchema()
{
    if (!m_enabled || !m_database.isValid() || !m_database.isOpen()) {
        return;
    }

    static const char* defaultPasswordHash =
        "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92";

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS operation_logs ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "user_id BIGINT NULL,"
        "module VARCHAR(64) NOT NULL,"
        "action VARCHAR(64) NOT NULL,"
        "content VARCHAR(500),"
        "ip_address VARCHAR(64),"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS registration_insurance_check ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "patient_id BIGINT NOT NULL,"
        "id_card VARCHAR(32) NOT NULL,"
        "hospital_area_code VARCHAR(16) NOT NULL DEFAULT '110100',"
        "insured_area_code VARCHAR(16) NOT NULL DEFAULT '110100',"
        "insu_status VARCHAR(32) NOT NULL,"
        "valid_start_date DATE NOT NULL,"
        "valid_end_date DATE NOT NULL,"
        "is_remote_filed TINYINT NOT NULL DEFAULT 1,"
        "arrears_months INT NOT NULL DEFAULT 0,"
        "benefit_suspended TINYINT NOT NULL DEFAULT 0,"
        "insurance_type VARCHAR(32) NOT NULL,"
        "outpatient_pooling_supported TINYINT NOT NULL DEFAULT 1,"
        "annual_quota_total DECIMAL(10,2) NOT NULL DEFAULT 0,"
        "annual_quota_used DECIMAL(10,2) NOT NULL DEFAULT 0,"
        "quota_year INT NOT NULL,"
        "data_version BIGINT NOT NULL DEFAULT 1,"
        "check_enabled TINYINT NOT NULL DEFAULT 1,"
        "expected_result_code VARCHAR(16) NOT NULL,"
        "remark VARCHAR(255),"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "UNIQUE KEY uk_registration_insurance_patient (patient_id),"
        "KEY idx_registration_insurance_id_card (id_card),"
        "FOREIGN KEY (patient_id) REFERENCES patients(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS registration_insurance_tokens ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "token_hash CHAR(64) NOT NULL UNIQUE,"
        "patient_id BIGINT NOT NULL,"
        "id_card VARCHAR(32) NOT NULL,"
        "hospital_area_code VARCHAR(16) NOT NULL,"
        "register_date DATE NOT NULL,"
        "department VARCHAR(64) NOT NULL,"
        "doctor VARCHAR(64) NOT NULL,"
        "time_slot VARCHAR(32) NOT NULL,"
        "data_version BIGINT NOT NULL,"
        "result_code VARCHAR(16) NOT NULL,"
        "expires_at DATETIME NOT NULL,"
        "used_at DATETIME NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "KEY idx_registration_insurance_token_patient (patient_id, expires_at),"
        "FOREIGN KEY (patient_id) REFERENCES patients(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS registration_insurance_audit_logs ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "stage VARCHAR(32) NOT NULL,"
        "patient_id BIGINT NULL,"
        "operator_id BIGINT NULL,"
        "request_params JSON NOT NULL,"
        "result_code VARCHAR(16) NOT NULL,"
        "data_version BIGINT NULL,"
        "log_description VARCHAR(500) NOT NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "KEY idx_registration_insurance_audit_patient (patient_id, created_at),"
        "FOREIGN KEY (patient_id) REFERENCES patients(id),"
        "FOREIGN KEY (operator_id) REFERENCES users(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN insurance_result_code VARCHAR(16) NULL AFTER fee");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN insurance_token_no VARCHAR(64) NULL AFTER insurance_result_code");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN payment_identity VARCHAR(32) NOT NULL DEFAULT 'SELF_PAY' AFTER insurance_token_no");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN is_emergency TINYINT NOT NULL DEFAULT 0 AFTER payment_identity");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN emergency_reason VARCHAR(255) NULL AFTER is_emergency");

    execIgnoreError(m_database,
        "UPDATE patients SET id_card = '110101199909210099' WHERE patient_no = 'P20260007' AND (id_card IS NULL OR id_card = '')");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101199603180021', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 3 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 1, "
        "0, 0, 'URBAN_EMPLOYEE', 1, 2000.00, 300.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_000', '正常参保，预期允许医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101199603180021' "
        "ON DUPLICATE KEY UPDATE insu_status = VALUES(insu_status), valid_end_date = VALUES(valid_end_date), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101198811020033', '110100', '110100', 'EXPIRED', DATE_SUB(CURRENT_DATE, INTERVAL 3 YEAR), DATE_SUB(CURRENT_DATE, INTERVAL 1 DAY), 1, "
        "0, 0, 'URBAN_EMPLOYEE', 1, 2000.00, 200.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_002', '医保已过期，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101198811020033' "
        "ON DUPLICATE KEY UPDATE insu_status = VALUES(insu_status), valid_end_date = VALUES(valid_end_date), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101201807090044', '110100', '310100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 0, "
        "0, 0, 'URBAN_RESIDENT', 1, 1200.00, 200.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_003', '异地未备案，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101201807090044' "
        "ON DUPLICATE KEY UPDATE insured_area_code = VALUES(insured_area_code), is_remote_filed = VALUES(is_remote_filed), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101197205060055', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 1, "
        "4, 1, 'URBAN_EMPLOYEE', 1, 2000.00, 100.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_004', '欠费停保，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101197205060055' "
        "ON DUPLICATE KEY UPDATE arrears_months = VALUES(arrears_months), benefit_suspended = VALUES(benefit_suspended), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101199112120066', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 1, "
        "0, 0, 'WORK_INJURY_ONLY', 0, 0.00, 0.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_005', '险种不支持门诊统筹，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101199112120066' "
        "ON DUPLICATE KEY UPDATE insurance_type = VALUES(insurance_type), outpatient_pooling_supported = VALUES(outpatient_pooling_supported), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101200102030077', '110100', '110100', 'ACTIVE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 1, "
        "0, 0, 'URBAN_EMPLOYEE', 1, 800.00, 800.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_006', '年度门诊统筹额度已用尽，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101200102030077' "
        "ON DUPLICATE KEY UPDATE annual_quota_total = VALUES(annual_quota_total), annual_quota_used = VALUES(annual_quota_used), data_version = data_version");

    execIgnoreError(m_database,
        "INSERT INTO registration_insurance_check "
        "(patient_id, id_card, hospital_area_code, insured_area_code, insu_status, valid_start_date, valid_end_date, is_remote_filed, "
        "arrears_months, benefit_suspended, insurance_type, outpatient_pooling_supported, annual_quota_total, annual_quota_used, "
        "quota_year, data_version, check_enabled, expected_result_code, remark) "
        "SELECT p.id, '110101199909210099', '110100', '110100', 'NO_INSURANCE', DATE_SUB(CURRENT_DATE, INTERVAL 2 YEAR), DATE_ADD(CURRENT_DATE, INTERVAL 1 YEAR), 1, "
        "0, 0, 'NONE', 0, 0.00, 0.00, YEAR(CURRENT_DATE), 1, 1, 'REG_INS_001', '无医保参保信息，预期阻断医保统筹挂号' "
        "FROM patients p WHERE p.id_card = '110101199909210099' "
        "ON DUPLICATE KEY UPDATE insu_status = VALUES(insu_status), insurance_type = VALUES(insurance_type), data_version = data_version");

    execIgnoreError(m_database,
        "ALTER TABLE bills MODIFY COLUMN status VARCHAR(32) NOT NULL DEFAULT 'UNPAID'");

    execIgnoreError(m_database,
        "ALTER TABLE bills ADD COLUMN pay_time DATETIME NULL AFTER status");

    execIgnoreError(m_database,
        "ALTER TABLE bills ADD COLUMN payment_token_hash CHAR(64) NULL AFTER pay_time");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS refund_requests ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "refund_no VARCHAR(64) NOT NULL UNIQUE,"
        "bill_id BIGINT NOT NULL,"
        "bill_no VARCHAR(64) NOT NULL,"
        "amount DECIMAL(10,2) NOT NULL,"
        "reason VARCHAR(500) NOT NULL,"
        "status VARCHAR(32) NOT NULL DEFAULT 'PENDING',"
        "requested_by BIGINT NULL,"
        "requested_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "reviewed_by BIGINT NULL,"
        "reviewed_at DATETIME NULL,"
        "review_note VARCHAR(500),"
        "INDEX idx_refund_bill (bill_id),"
        "INDEX idx_refund_status (status),"
        "FOREIGN KEY (bill_id) REFERENCES bills(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN reviewer_id BIGINT NULL AFTER total_amount");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN review_time DATETIME NULL AFTER reviewer_id");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN reject_reason VARCHAR(500) NULL AFTER review_time");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN dispense_user_id BIGINT NULL AFTER reject_reason");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN dispense_time DATETIME NULL AFTER dispense_user_id");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN return_user_id BIGINT NULL AFTER dispense_time");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN return_time DATETIME NULL AFTER return_user_id");
    execIgnoreError(m_database,
        "ALTER TABLE prescriptions ADD COLUMN return_reason VARCHAR(500) NULL AFTER return_time");

    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN present_illness VARCHAR(1000) NULL AFTER chief_complaint");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN past_history VARCHAR(1000) NULL AFTER present_illness");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN physical_sign VARCHAR(1000) NULL AFTER past_history");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN icd_code VARCHAR(32) NULL AFTER physical_sign");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_hospital VARCHAR(128) NULL AFTER advice");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_type VARCHAR(64) NULL AFTER external_report_hospital");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_date DATE NULL AFTER external_report_type");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_summary VARCHAR(1000) NULL AFTER external_report_date");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_conclusion VARCHAR(1000) NULL AFTER external_report_summary");
    execIgnoreError(m_database,
        "ALTER TABLE medical_records ADD COLUMN external_report_attachment VARCHAR(500) NULL AFTER external_report_conclusion");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS examination_items ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "item_code VARCHAR(32) NOT NULL UNIQUE,"
        "item_name VARCHAR(128) NOT NULL UNIQUE,"
        "category VARCHAR(64) NOT NULL DEFAULT '检查',"
        "unit_price DECIMAL(10,2) NOT NULL DEFAULT 0,"
        "status TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "ALTER TABLE examinations ADD COLUMN item_id BIGINT NULL AFTER doctor_id");
    execIgnoreError(m_database,
        "ALTER TABLE examinations ADD COLUMN unit_price DECIMAL(10,2) NOT NULL DEFAULT 0 AFTER item_name");
    execIgnoreError(m_database,
        "ALTER TABLE examinations ADD COLUMN report_finding VARCHAR(1000) NULL AFTER result_text");
    execIgnoreError(m_database,
        "ALTER TABLE examinations ADD COLUMN report_conclusion VARCHAR(1000) NULL AFTER report_finding");
    execIgnoreError(m_database,
        "ALTER TABLE examinations ADD COLUMN report_attachment VARCHAR(500) NULL AFTER report_conclusion");

    execIgnoreError(m_database,
        "INSERT INTO examination_items (item_code, item_name, category, unit_price, status) VALUES "
        "('EXAM_CT', 'CT', '影像检查', 260.00, 1),"
        "('EXAM_XRAY', 'X线', '影像检查', 80.00, 1),"
        "('EXAM_CHEST_XRAY', '胸片', '影像检查', 90.00, 1),"
        "('EXAM_BLOOD', '血常规', '检验', 25.00, 1),"
        "('EXAM_URINE', '尿常规', '检验', 18.00, 1),"
        "('EXAM_US', '腹部彩超', '超声', 120.00, 1),"
        "('EXAM_ECG', '心电图', '功能检查', 35.00, 1) "
        "ON DUPLICATE KEY UPDATE unit_price = VALUES(unit_price), status = VALUES(status)");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS insurance_transactions ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "transaction_id VARCHAR(64) NOT NULL UNIQUE,"
        "bill_id BIGINT NOT NULL,"
        "patient_id BIGINT NOT NULL,"
        "amount DECIMAL(10,2) NOT NULL,"
        "status VARCHAR(16) NOT NULL DEFAULT 'PROCESSING',"
        "request_payload JSON NOT NULL,"
        "last_error VARCHAR(500),"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "INDEX idx_insurance_bill (bill_id),"
        "INDEX idx_insurance_status (status),"
        "FOREIGN KEY (bill_id) REFERENCES bills(id),"
        "FOREIGN KEY (patient_id) REFERENCES patients(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "ALTER TABLE payments ADD UNIQUE KEY uk_payments_bill (bill_id)");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS audit_log_details ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "operation_log_id BIGINT NULL,"
        "user_id BIGINT NULL,"
        "module VARCHAR(64) NOT NULL,"
        "action VARCHAR(64) NOT NULL,"
        "business_key VARCHAR(128),"
        "field_name VARCHAR(64) NOT NULL,"
        "old_value VARCHAR(1000),"
        "new_value VARCHAR(1000),"
        "change_reason VARCHAR(255),"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS pass_rules ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "rule_code VARCHAR(32) NOT NULL UNIQUE,"
        "rule_type VARCHAR(32) NOT NULL,"
        "drug_name VARCHAR(128) NOT NULL,"
        "related_drug_name VARCHAR(128),"
        "patient_condition VARCHAR(128),"
        "warning_level VARCHAR(16) NOT NULL DEFAULT 'WARN',"
        "message VARCHAR(500) NOT NULL,"
        "enabled TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS examinations ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "examination_no VARCHAR(32) NOT NULL UNIQUE,"
        "registration_id BIGINT NOT NULL,"
        "doctor_id BIGINT NOT NULL,"
        "item_name VARCHAR(128) NOT NULL,"
        "request_note VARCHAR(500),"
        "result_text VARCHAR(1000),"
        "status VARCHAR(16) NOT NULL DEFAULT 'PENDING',"
        "request_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "complete_time DATETIME NULL,"
        "INDEX idx_exam_registration (registration_id),"
        "INDEX idx_exam_doctor (doctor_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS schedule_rules ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "rule_code VARCHAR(32) NOT NULL UNIQUE,"
        "rule_type VARCHAR(16) NOT NULL,"
        "target_type VARCHAR(16) NOT NULL,"
        "doctor_id BIGINT NULL,"
        "department_id BIGINT NULL,"
        "title VARCHAR(64) NULL,"
        "target_text VARCHAR(128) NULL,"
        "date_mode VARCHAR(16) NOT NULL,"
        "weekdays_mask INT NOT NULL DEFAULT 0,"
        "start_date DATE NULL,"
        "end_date DATE NULL,"
        "reason VARCHAR(255),"
        "raw_text VARCHAR(500),"
        "enabled TINYINT NOT NULL DEFAULT 1,"
        "created_by BIGINT NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "INDEX idx_schedule_rules_enabled (enabled),"
        "INDEX idx_schedule_rules_doctor (doctor_id, enabled),"
        "INDEX idx_schedule_rules_department (department_id, enabled),"
        "INDEX idx_schedule_rules_date (date_mode, start_date, end_date),"
        "FOREIGN KEY (doctor_id) REFERENCES doctors(id),"
        "FOREIGN KEY (department_id) REFERENCES departments(id),"
        "FOREIGN KEY (created_by) REFERENCES users(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS outbox_events ("
        "id BIGINT NOT NULL AUTO_INCREMENT,"
        "event_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,"
        "dedupe_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NULL,"
        "event_type VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,"
        "aggregate_type VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,"
        "aggregate_id BIGINT NOT NULL,"
        "business_key VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,"
        "route_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_general_ci NULL,"
        "payload JSON NOT NULL,"
        "headers JSON NULL,"
        "status TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        "retry_count SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        "max_retry SMALLINT UNSIGNED NOT NULL DEFAULT 10,"
        "next_retry_at DATETIME(3) NULL,"
        "locked_by VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NULL,"
        "locked_at DATETIME(3) NULL,"
        "last_error VARCHAR(1000) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,"
        "created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),"
        "updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),"
        "published_at DATETIME(3) NULL,"
        "PRIMARY KEY (id),"
        "UNIQUE KEY uk_outbox_event_id (event_id),"
        "UNIQUE KEY uk_outbox_dedupe_key (dedupe_key),"
        "KEY idx_outbox_poll (status, next_retry_at, id),"
        "KEY idx_outbox_lock_recovery (status, locked_at),"
        "KEY idx_outbox_aggregate_order (aggregate_type, aggregate_id, id),"
        "KEY idx_outbox_business_key (business_key),"
        "KEY idx_outbox_route_status (route_key, status, id),"
        "KEY idx_outbox_published_cleanup (status, published_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_dept ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "dept_code VARCHAR(32) NOT NULL UNIQUE,"
        "dept_name VARCHAR(64) NOT NULL,"
        "parent_id BIGINT NULL,"
        "status TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "INDEX idx_sys_dept_parent (parent_id),"
        "FOREIGN KEY (parent_id) REFERENCES sys_dept(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_user ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "username VARCHAR(64) NOT NULL UNIQUE,"
        "password_hash VARCHAR(255) NOT NULL,"
        "real_name VARCHAR(64) NOT NULL,"
        "phone VARCHAR(32),"
        "status TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_role ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "role_code VARCHAR(32) NOT NULL UNIQUE,"
        "role_name VARCHAR(64) NOT NULL,"
        "data_scope VARCHAR(32) NOT NULL DEFAULT 'SELF',"
        "description VARCHAR(255),"
        "status TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_menu ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "parent_id BIGINT NULL,"
        "menu_code VARCHAR(64) NOT NULL UNIQUE,"
        "menu_name VARCHAR(64) NOT NULL,"
        "menu_type VARCHAR(16) NOT NULL,"
        "permission_code VARCHAR(128) UNIQUE,"
        "module_code VARCHAR(64),"
        "action_code VARCHAR(64),"
        "status TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "INDEX idx_sys_menu_parent (parent_id),"
        "FOREIGN KEY (parent_id) REFERENCES sys_menu(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_user_role ("
        "user_id BIGINT NOT NULL,"
        "role_id BIGINT NOT NULL,"
        "is_primary TINYINT NOT NULL DEFAULT 1,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (user_id, role_id),"
        "FOREIGN KEY (user_id) REFERENCES sys_user(id),"
        "FOREIGN KEY (role_id) REFERENCES sys_role(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_role_menu ("
        "role_id BIGINT NOT NULL,"
        "menu_id BIGINT NOT NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (role_id, menu_id),"
        "FOREIGN KEY (role_id) REFERENCES sys_role(id),"
        "FOREIGN KEY (menu_id) REFERENCES sys_menu(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS sys_user_dept ("
        "user_id BIGINT NOT NULL,"
        "dept_id BIGINT NOT NULL,"
        "is_primary TINYINT NOT NULL DEFAULT 0,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (user_id, dept_id),"
        "FOREIGN KEY (user_id) REFERENCES sys_user(id),"
        "FOREIGN KEY (dept_id) REFERENCES sys_dept(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role (role_code, role_name, data_scope, description) VALUES "
        "('ADMIN', '系统管理员', 'ALL', '全院后台管理权限'),"
        "('DIRECTOR', '科主任', 'DEPT', '本科室诊疗和排班管理'),"
        "('REGISTRAR', '挂号员', 'ALL', '患者建档和挂号操作'),"
        "('DOCTOR', '医生', 'SELF', '本人接诊和处方操作'),"
        "('PHARMACIST', '药房人员', 'PHARMACY_PENDING', '待审核和待发药处方处理'),"
        "('CASHIER', '收费员', 'ALL', '收费结算和退费操作')");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_menu "
        "(menu_code, menu_name, menu_type, permission_code, module_code, action_code) VALUES "
        "('patient_list', '查看患者', 'API', 'patient:list', 'patient', 'list'),"
        "('patient_update', '修改患者', 'API', 'patient:update', 'patient', 'update'),"
        "('patient_delete', '停用患者', 'API', 'patient:delete', 'patient', 'delete'),"
        "('registration_waiting', '候诊队列', 'API', 'registration:waiting', 'registration', 'waiting'),"
        "('registration_call', '叫号', 'API', 'registration:call', 'registration', 'call'),"
        "('registration_mark_emergency', '急诊优先', 'API', 'registration:markEmergency', 'registration', 'markEmergency'),"
        "('registration_history', '挂号历史', 'API', 'registration:history', 'registration', 'history'),"
        "('registration_update', '修改挂号', 'API', 'registration:update', 'registration', 'update'),"
        "('registration_delete', '退停挂号', 'API', 'registration:delete', 'registration', 'delete'),"
        "('registration_insurance_precheck', '医保预检', 'API', 'registration:insurancePrecheck', 'registration', 'insurancePrecheck'),"
        "('registration_insurance_profile', '医保档案', 'API', 'registration:insuranceProfile', 'registration', 'insuranceProfile'),"
        "('registration_save_insurance_profile', '保存医保档案', 'API', 'registration:saveInsuranceProfile', 'registration', 'saveInsuranceProfile'),"
        "('schedule_list', '查看排班', 'API', 'schedule:list', 'schedule', 'list'),"
        "('schedule_range_list', '范围排班', 'API', 'schedule:rangeList', 'schedule', 'rangeList'),"
        "('schedule_save', '保存排班', 'API', 'schedule:save', 'schedule', 'save'),"
        "('schedule_update', '修改排班', 'API', 'schedule:update', 'schedule', 'update'),"
        "('schedule_delete', '停用排班', 'API', 'schedule:delete', 'schedule', 'delete'),"
        "('schedule_reset', '重置排班', 'API', 'schedule:reset', 'schedule', 'reset'),"
        "('schedule_batch_save', '批量排班', 'API', 'schedule:batchSave', 'schedule', 'batchSave'),"
        "('schedule_rules_list', '排班规则', 'API', 'schedule:rulesList', 'schedule', 'rulesList'),"
        "('schedule_rules_save_all', '保存排班规则', 'API', 'schedule:rulesSaveAll', 'schedule', 'rulesSaveAll'),"
        "('department_list', '查看科室', 'API', 'department:list', 'department', 'list'),"
        "('doctor_list', '查看医生', 'API', 'doctor:list', 'doctor', 'list'),"
        "('doctor_create', '新增医生', 'API', 'doctor:create', 'doctor', 'create'),"
        "('doctor_update', '修改医生', 'API', 'doctor:update', 'doctor', 'update'),"
        "('doctor_delete', '停用医生', 'API', 'doctor:delete', 'doctor', 'delete'),"
        "('registration_create', '创建挂号', 'API', 'registration:create', 'registration', 'create'),"
        "('registration_list', '查看挂号', 'API', 'registration:list', 'registration', 'list'),"
        "('consultation_list', '查看接诊', 'API', 'consultation:list', 'consultation', 'list'),"
        "('consultation_start', '开始接诊', 'API', 'consultation:start', 'consultation', 'start'),"
        "('consultation_save', '保存接诊', 'API', 'consultation:save', 'consultation', 'save'),"
        "('consultation_save_waiting', '保存候诊接诊', 'API', 'consultation:saveWaiting', 'consultation', 'saveWaiting'),"
        "('examination_list', '查看检查', 'API', 'examination:list', 'examination', 'list'),"
        "('examination_items', '查看检查项目', 'API', 'examination:items', 'examination', 'items'),"
        "('examination_save_item', '维护检查项目', 'API', 'examination:saveItem', 'examination', 'saveItem'),"
        "('examination_delete_item', '停用检查项目', 'API', 'examination:deleteItem', 'examination', 'deleteItem'),"
        "('examination_create', '申请检查', 'API', 'examination:create', 'examination', 'create'),"
        "('examination_complete', '完成检查', 'API', 'examination:complete', 'examination', 'complete'),"
        "('prescription_list', '查看处方', 'API', 'prescription:list', 'prescription', 'list'),"
        "('prescription_create', '开立处方', 'API', 'prescription:create', 'prescription', 'create'),"
        "('prescription_review', '审核处方', 'API', 'prescription:review', 'prescription', 'review'),"
        "('prescription_reject', '驳回处方', 'API', 'prescription:reject', 'prescription', 'reject'),"
        "('prescription_dispense', '发药', 'API', 'prescription:dispense', 'prescription', 'dispense'),"
        "('prescription_return', '退药入库', 'API', 'prescription:return', 'prescription', 'return'),"
        "('inventory_list', '查看库存', 'API', 'inventory:list', 'inventory', 'list'),"
        "('inventory_inbound', '药品入库', 'API', 'inventory:inbound', 'inventory', 'inbound'),"
        "('inventory_update', '维护库存', 'API', 'inventory:update', 'inventory', 'update'),"
        "('inventory_delete', '停用药品', 'API', 'inventory:delete', 'inventory', 'delete'),"
        "('billing_list', '查看收费', 'API', 'billing:list', 'billing', 'list'),"
        "('billing_pay', '收费', 'API', 'billing:pay', 'billing', 'pay'),"
        "('billing_medical_insurance_pay', '医保收费', 'API', 'billing:medicalInsurancePay', 'billing', 'medicalInsurancePay'),"
        "('billing_create_payment_qr', '创建支付码', 'API', 'billing:createPaymentQr', 'billing', 'createPaymentQr'),"
        "('billing_check_pay_status', '检查支付状态', 'API', 'billing:checkPayStatus', 'billing', 'checkPayStatus'),"
        "('billing_refund', '退费', 'API', 'billing:refund', 'billing', 'refund'),"
        "('billing_request_refund', '申请退费', 'API', 'billing:requestRefund', 'billing', 'requestRefund'),"
        "('billing_review_refund', '审核退费', 'API', 'billing:reviewRefund', 'billing', 'reviewRefund'),"
        "('billing_update', '修改账单', 'API', 'billing:update', 'billing', 'update'),"
        "('statistics_daily', '费用统计', 'API', 'statistics:daily', 'statistics', 'daily'),"
        "('dashboard_summary', '驾驶舱概览', 'API', 'dashboard:summary', 'dashboard', 'summary'),"
        "('dashboard_stats', '驾驶舱统计', 'API', 'dashboard:stats', 'dashboard', 'stats'),"
        "('dashboard_top_doctors', '医生排行', 'API', 'dashboard:topDoctors', 'dashboard', 'topDoctors'),"
        "('operation_log_list', '查看操作日志', 'API', 'operationLog:list', 'operationLog', 'list'),"
        "('patient_record_list', '查看病历档案', 'API', 'patientRecord:list', 'patientRecord', 'list'),"
        "('patient_record_update', '修改病历档案', 'API', 'patientRecord:update', 'patientRecord', 'update'),"
        "('patient_record_delete', '删除病历档案', 'API', 'patientRecord:delete', 'patientRecord', 'delete'),"
        "('permission_admin_users', '账号管理', 'API', 'permissionAdmin:users', 'permissionAdmin', 'users'),"
        "('permission_admin_roles', '角色权限', 'API', 'permissionAdmin:roles', 'permissionAdmin', 'roles'),"
        "('permission_admin_create_user', '新增账号', 'API', 'permissionAdmin:createUser', 'permissionAdmin', 'createUser'),"
        "('permission_admin_reset_password', '重置密码', 'API', 'permissionAdmin:resetPassword', 'permissionAdmin', 'resetPassword'),"
        "('permission_admin_toggle_user', '启停账号', 'API', 'permissionAdmin:toggleUser', 'permissionAdmin', 'toggleUser'),"
        "('permission_admin_save_role_permissions', '保存角色权限', 'API', 'permissionAdmin:saveRolePermissions', 'permissionAdmin', 'saveRolePermissions')");

    syncLegacyUsersToRbac(m_database, QString::fromLatin1(defaultPasswordHash));

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'ADMIN' "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'ADMIN' "
        "AND m.permission_code = 'operationLog:list'");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'ADMIN' "
        "AND m.permission_code IN ('billing:list','billing:requestRefund','billing:reviewRefund')");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'ADMIN' "
        "AND m.permission_code IN ('examination:items','examination:saveItem','examination:deleteItem')");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'DIRECTOR' "
        "AND m.permission_code IN ("
        "'dashboard:summary','dashboard:stats','dashboard:topDoctors','statistics:daily',"
        "'patient:list','patient:update','patient:delete','registration:list','registration:waiting','registration:call',"
        "'consultation:list','consultation:start','consultation:save','consultation:saveWaiting',"
        "'examination:list','examination:items','examination:saveItem','examination:deleteItem',"
        "'examination:create','examination:complete','prescription:list','prescription:create',"
        "'prescription:review','prescription:reject','prescription:dispense','prescription:return',"
        "'patientRecord:list','patientRecord:update','patientRecord:delete','doctor:list','doctor:create','doctor:update','doctor:delete',"
        "'schedule:list','schedule:rangeList','schedule:save','schedule:update','schedule:delete','schedule:reset','schedule:batchSave','schedule:rulesList','schedule:rulesSaveAll') "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'REGISTRAR' "
        "AND m.permission_code IN ("
        "'patient:list','patient:update','patient:delete','registration:list','registration:create','registration:insurancePrecheck',"
        "'registration:insuranceProfile','registration:saveInsuranceProfile','registration:update','registration:delete',"
        "'registration:waiting','registration:call','registration:history','billing:pay','billing:medicalInsurancePay',"
        "'billing:createPaymentQr','billing:checkPayStatus','schedule:list','schedule:rangeList','schedule:save','schedule:update',"
        "'schedule:delete','schedule:reset','schedule:batchSave','schedule:rulesList','schedule:rulesSaveAll','department:list','doctor:list','patientRecord:list') "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'DOCTOR' "
        "AND m.permission_code IN ("
        "'patient:list','registration:waiting','registration:call','consultation:list','consultation:start',"
        "'consultation:save','consultation:saveWaiting','examination:list','examination:items','examination:create','examination:complete',"
        "'prescription:list','prescription:create','patientRecord:list','patientRecord:update','doctor:list') "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'PHARMACIST' "
        "AND m.permission_code IN ("
        "'prescription:list','prescription:review','prescription:reject','prescription:dispense','prescription:return',"
        "'inventory:list','inventory:inbound','inventory:update','inventory:delete') "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO sys_role_menu (role_id, menu_id) "
        "SELECT r.id, m.id FROM sys_role r JOIN sys_menu m "
        "WHERE r.role_code = 'CASHIER' "
        "AND m.permission_code IN ("
        "'billing:list','billing:pay','billing:medicalInsurancePay','billing:createPaymentQr','billing:checkPayStatus',"
        "'billing:refund','billing:requestRefund','billing:update',"
        "'statistics:daily','dashboard:summary','dashboard:stats','dashboard:topDoctors') "
        "AND NOT EXISTS (SELECT 1 FROM sys_role_menu existing WHERE existing.role_id = r.id)");

    execIgnoreError(m_database,
        "DELETE srm FROM sys_role_menu srm "
        "JOIN sys_role r ON r.id = srm.role_id "
        "JOIN sys_menu m ON m.id = srm.menu_id "
        "WHERE r.role_code = 'CASHIER' AND m.permission_code = 'billing:reviewRefund'");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO pass_rules "
        "(rule_code, rule_type, drug_name, related_drug_name, patient_condition, warning_level, message) VALUES "
        "('PASS001', 'ALLERGY', '阿莫西林胶囊', NULL, '青霉素过敏', 'BLOCK', '患者存在青霉素过敏风险，禁止开立阿莫西林类药品。'),"
        "('PASS002', 'DOSE', '布洛芬缓释胶囊', NULL, '儿童', 'WARN', '儿童使用布洛芬需核对年龄和体重，避免超剂量。'),"
        "('PASS003', 'COMBO', '阿莫西林胶囊', '布洛芬缓释胶囊', NULL, 'WARN', '抗生素与解热镇痛药联合使用时需确认感染指征和胃肠道风险。')");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO roles (role_code, role_name, description) VALUES "
        "('DIRECTOR', '科主任', '查看本科室经营和诊疗数据')");

    QSqlQuery ensureDirector(m_database);
    ensureDirector.prepare(
        "INSERT IGNORE INTO users (username, password_hash, real_name, phone, role_id, status) "
        "SELECT 'director01', :password_hash, '内科门诊主任', '13800000007', id, 1 "
        "FROM roles WHERE role_code = 'DIRECTOR' LIMIT 1");
    ensureDirector.bindValue(":password_hash", QString::fromLatin1(defaultPasswordHash));
    ensureDirector.exec();
    ensureDirector.finish();

    QSqlQuery resetDefaultAccounts(m_database);
    resetDefaultAccounts.prepare(
        "UPDATE users SET password_hash = :password_hash, status = 1 "
        "WHERE username IN ('admin', 'director01', 'reg01', 'doctor01', 'doctor02', 'doctor03', "
        "'doctor04', 'doctor05', 'doctor06', 'pharmacy01', 'cashier01')");
    resetDefaultAccounts.bindValue(":password_hash", QString::fromLatin1(defaultPasswordHash));
    resetDefaultAccounts.exec();
    resetDefaultAccounts.finish();

    execIgnoreError(m_database,
        "ALTER TABLE drugs ADD COLUMN expiry_date DATE NULL AFTER warning_quantity");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN appointment_time_slot VARCHAR(32) NULL AFTER schedule_id");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS patient_users ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "username VARCHAR(64) NOT NULL UNIQUE,"
        "password_hash VARCHAR(255) NOT NULL,"
        "password_salt VARCHAR(64) NOT NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD COLUMN user_id BIGINT NULL AFTER patient_no");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD COLUMN username VARCHAR(64) NULL AFTER patient_no");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD COLUMN password_hash VARCHAR(255) NULL AFTER username");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD COLUMN password_salt VARCHAR(64) NULL AFTER password_hash");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD COLUMN relationship VARCHAR(32) DEFAULT '本人' AFTER phone");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD UNIQUE KEY uk_patients_username (username)");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD UNIQUE KEY uk_patients_user_id_card (user_id, id_card)");

    execIgnoreError(m_database,
        "ALTER TABLE patients ADD CONSTRAINT fk_patients_user FOREIGN KEY (user_id) REFERENCES patient_users(id)");

    execIgnoreError(m_database,
        "ALTER TABLE registrations ADD COLUMN user_id BIGINT NULL AFTER registration_no");

    execIgnoreError(m_database,
        "ALTER TABLE bills ADD COLUMN user_id BIGINT NULL AFTER bill_no");

    execIgnoreError(m_database,
        "UPDATE patients "
        "SET username = COALESCE(NULLIF(phone, ''), patient_no), "
        "password_salt = patient_no, "
        "password_hash = SHA2(CONCAT(password_salt, ':123456'), 256) "
        "WHERE username IS NULL OR username = '' OR password_hash IS NULL OR password_hash = ''");

    execIgnoreError(m_database,
        "INSERT IGNORE INTO patient_users (username, password_salt, password_hash) "
        "SELECT DISTINCT username, password_salt, password_hash "
        "FROM patients WHERE username IS NOT NULL AND username <> ''");

    execIgnoreError(m_database,
        "UPDATE patients p "
        "JOIN patient_users u ON u.username = p.username "
        "SET p.user_id = u.id "
        "WHERE p.user_id IS NULL");

    execIgnoreError(m_database,
        "UPDATE registrations r "
        "JOIN patients p ON p.id = r.patient_id "
        "SET r.user_id = p.user_id "
        "WHERE r.user_id IS NULL");

    execIgnoreError(m_database,
        "UPDATE bills b "
        "JOIN patients p ON p.id = b.patient_id "
        "SET b.user_id = p.user_id "
        "WHERE b.user_id IS NULL");

    execIgnoreError(m_database,
        "UPDATE registrations r "
        "JOIN doctor_schedules s ON s.id = r.schedule_id "
        "SET r.appointment_time_slot = s.period "
        "WHERE r.appointment_time_slot IS NULL OR r.appointment_time_slot = ''");

    execIgnoreError(m_database,
        "CREATE TABLE IF NOT EXISTS fee_statistics_daily ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "stat_date DATE NOT NULL,"
        "department_id BIGINT NULL,"
        "registration_income DECIMAL(12,2) NOT NULL DEFAULT 0,"
        "drug_income DECIMAL(12,2) NOT NULL DEFAULT 0,"
        "total_income DECIMAL(12,2) NOT NULL DEFAULT 0,"
        "UNIQUE KEY uk_daily_department (stat_date, department_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    bool hasScheduleDepartment = false;
    {
        QSqlQuery check(m_database);
        check.prepare("SELECT COUNT(*) FROM information_schema.COLUMNS "
                      "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'doctor_schedules' "
                      "AND COLUMN_NAME = 'department_id'");
        hasScheduleDepartment = check.exec() && check.next() && check.value(0).toInt() > 0;
        check.finish();
    }

    if (!hasScheduleDepartment) {
        QSqlQuery alter(m_database);
        const bool altered = alter.exec("ALTER TABLE doctor_schedules ADD COLUMN department_id BIGINT NULL AFTER doctor_id");
        alter.finish();
        if (!altered) {
            return;
        }
    }

    {
        QSqlQuery backfill(m_database);
        backfill.exec("UPDATE doctor_schedules s "
                      "JOIN doctors doc ON doc.id = s.doctor_id "
                      "SET s.department_id = doc.department_id "
                      "WHERE s.department_id IS NULL");
        backfill.finish();
    }

    if (!hasScheduleDepartment) {
        QSqlQuery fk(m_database);
        fk.exec("ALTER TABLE doctor_schedules "
                "ADD CONSTRAINT fk_schedule_department FOREIGN KEY (department_id) REFERENCES departments(id)");
        fk.finish();
    }

    QSqlQuery normalizeDoctors(m_database);
    normalizeDoctors.exec("UPDATE doctors doc "
                          "JOIN departments clinic ON clinic.id = doc.department_id "
                          "JOIN departments specialty ON specialty.dept_code = LEFT(clinic.dept_code, 8) "
                          "SET doc.department_id = specialty.id "
                          "WHERE clinic.dept_code LIKE 'DEP%' AND LENGTH(clinic.dept_code) = 10 "
                          "AND LENGTH(specialty.dept_code) = 8");
    normalizeDoctors.finish();

    seedCatalogDoctors(m_database);
    syncLegacyUsersToRbac(m_database, QString::fromLatin1(defaultPasswordHash));
}

} // namespace hospital::server
