#include "server/modules/ModuleServices.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"
#include "server/SqlJson.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantMap>

namespace hospital::server {
namespace {

common::Response updatePatient(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("UPDATE patients SET name = :name, gender = :gender, phone = :phone, "
                  "id_card = :id_card, address = :address WHERE patient_no = :patient_no");
    query.bindValue(":name", payload.value("姓名").toString());
    query.bindValue(":gender", payload.value("性别").toString());
    query.bindValue(":phone", payload.value("电话").toString());
    query.bindValue(":id_card", payload.value("身份证号").toString());
    query.bindValue(":address", payload.value("地址").toString());
    query.bindValue(":patient_no", payload.value("患者编号").toString());
    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }
    return {true, "患者信息已修改。", {}};
}

common::Response deletePatient(DatabaseManager* database, const QJsonObject& payload)
{
    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare("DELETE FROM patients WHERE patient_no = :patient_no");
    query.bindValue(":patient_no", payload.value("患者编号").toString());
    if (!query.exec()) {
        return {false, "该患者已有挂号/账单等关联记录，不能直接删除，可在论文中说明为保护病历数据。" + query.lastError().text(), {}};
    }
    return {true, "患者信息已删除。", {}};
}

} // namespace

PatientService::PatientService(DatabaseManager* database)
    : m_database(database)
{
}

common::Response PatientService::handle(const common::Request& request)
{
    if (request.action == "update") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().updatePatient(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return updatePatient(m_database, request.payload);
    }
    if (request.action == "delete") {
        if (!m_database->isEnabled()) {
            const auto result = DemoRepository::instance().deletePatient(request.payload);
            return {result.value("success").toBool(), result.value("message").toString(), result};
        }
        return deletePatient(m_database, request.payload);
    }
    if (request.action != "list") {
        return {false, "Unsupported patient action", {}};
    }

    const QString keyword = request.payload.value("keyword").toString().trimmed();
    const QString operatorRoleCode = request.payload.value("__operatorRoleCode").toString();
    const QString operatorName = request.payload.value("__operatorName").toString().trimmed();
    QStringList filters;
    QVariantMap params{{"keyword", keyword}};
    if (operatorRoleCode == "DOCTOR" && !operatorName.isEmpty()) {
        filters.append("u.real_name = :operator_name");
        params.insert("operator_name", operatorName);
    } else if (operatorRoleCode == "DIRECTOR" && !operatorName.isEmpty()) {
        filters.append("(:operator_dept LIKE CONCAT('%', d.dept_name, '%') "
                       "OR (:operator_internal LIKE '%内科%' AND d.dept_code LIKE 'DEP001%') "
                       "OR (:operator_surgery LIKE '%外科%' AND d.dept_code LIKE 'DEP002%') "
                       "OR (:operator_pediatrics LIKE '%儿科%' AND d.dept_code LIKE 'DEP003%') "
                       "OR (:operator_tcm LIKE '%中医%' AND d.dept_code LIKE 'DEP004%'))");
        params.insert("operator_dept", operatorName);
        params.insert("operator_internal", operatorName);
        params.insert("operator_surgery", operatorName);
        params.insert("operator_pediatrics", operatorName);
        params.insert("operator_tcm", operatorName);
    }
    if (!keyword.isEmpty()) {
        filters.append("(p.patient_no LIKE CONCAT('%', :keyword_no, '%') "
                       "OR p.name LIKE CONCAT('%', :keyword_name, '%') "
                       "OR p.phone LIKE CONCAT('%', :keyword_phone, '%') "
                       "OR p.id_card LIKE CONCAT('%', :keyword_id_card, '%'))");
        params.insert("keyword_no", keyword);
        params.insert("keyword_name", keyword);
        params.insert("keyword_phone", keyword);
        params.insert("keyword_id_card", keyword);
    }
    const QString whereSql = filters.isEmpty() ? QString() : "WHERE " + filters.join(" AND ") + " ";

    return SqlJson::selectRows(m_database,
        "SELECT p.patient_no AS '患者编号', p.name AS '姓名', p.gender AS '性别', "
        "p.phone AS '电话', p.id_card AS '身份证号', "
        "CASE WHEN COALESCE(p.id_card, '') <> '' OR COALESCE(p.phone, '') <> '' THEN '已登记' ELSE '待补全' END AS '身份登记', "
        "CASE WHEN COUNT(r.id) > 0 THEN '已确认患者' ELSE '仅建档' END AS '患者状态', "
        "COUNT(r.id) AS '就诊次数', COALESCE(MAX(r.register_time), '') AS '最近就诊', "
        "CONCAT((CASE WHEN COALESCE(p.phone, '') <> '' THEN 25 ELSE 0 END "
        "+ CASE WHEN COALESCE(p.id_card, '') <> '' THEN 25 ELSE 0 END "
        "+ CASE WHEN COALESCE(p.address, '') <> '' THEN 20 ELSE 0 END "
        "+ CASE WHEN COUNT(r.id) > 0 THEN 30 ELSE 0 END), '%') AS '档案完整度', "
        "CASE WHEN COUNT(r.id) >= 2 THEN '复诊患者' "
        "WHEN COALESCE(p.phone, '') = '' OR COALESCE(p.id_card, '') = '' THEN '资料待补全' "
        "WHEN COUNT(r.id) = 1 THEN '首次就诊已确认' ELSE '新建档案' END AS '智能提示', "
        "p.address AS '地址', p.created_at AS '建档时间' "
        "FROM patients p "
        "LEFT JOIN registrations r ON r.patient_id = p.id AND r.status <> 'CANCELLED' "
        "LEFT JOIN doctors doc ON doc.id = r.doctor_id "
        "LEFT JOIN users u ON u.id = doc.user_id "
        "LEFT JOIN departments d ON d.id = doc.department_id "
        + whereSql +
        "GROUP BY p.id, p.patient_no, p.name, p.gender, p.phone, p.id_card, p.address, p.created_at "
        "ORDER BY p.created_at DESC LIMIT 100",
        params, "patients");
}

} // namespace hospital::server
