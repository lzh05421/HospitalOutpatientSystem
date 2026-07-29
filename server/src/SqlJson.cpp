#include "server/SqlJson.h"

#include "server/DatabaseManager.h"
#include "server/DemoRepository.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTime>

namespace hospital::server {
namespace {

QJsonObject row(std::initializer_list<std::pair<QString, QJsonValue>> values)
{
    QJsonObject object;
    for (const auto& value : values) {
        object.insert(value.first, value.second);
    }
    return object;
}

common::Response demoResponse(const QString& key)
{
    QJsonArray rows;

    if (key == "patients") {
        rows.append(row({{"患者编号", "P20260001"}, {"姓名", "王小兰"}, {"性别", "女"}, {"电话", "13910000001"}, {"身份证号", "110101199603180021"}, {"地址", "北京市海淀区"}, {"建档时间", "2026-05-28 09:10:00"}}));
        rows.append(row({{"患者编号", "P20260002"}, {"姓名", "赵强"}, {"性别", "男"}, {"电话", "13910000002"}, {"身份证号", "110101198811020033"}, {"地址", "北京市朝阳区"}, {"建档时间", "2026-05-28 09:25:00"}}));
        rows.append(row({{"患者编号", "P20260003"}, {"姓名", "陈晨"}, {"性别", "女"}, {"电话", "13910000003"}, {"身份证号", "110101201807090044"}, {"地址", "北京市西城区"}, {"建档时间", "2026-05-28 10:05:00"}}));
    } else if (key == "registrations") {
        rows.append(row({{"挂号单号", "R202605280001"}, {"患者", "王小兰"}, {"科室", "心血管内科诊室"}, {"医生", "张明"}, {"就诊日期", "2026-05-28"}, {"时段", "09:00-09:30"}, {"状态", "WAITING"}, {"挂号费", 23.00}, {"挂号时间", "2026-05-28 09:30:00"}}));
        rows.append(row({{"挂号单号", "R202605280002"}, {"患者", "赵强"}, {"科室", "普外科诊室"}, {"医生", "李华"}, {"就诊日期", "2026-05-28"}, {"时段", "09:30-10:00"}, {"状态", "FINISHED"}, {"挂号费", 18.00}, {"挂号时间", "2026-05-28 09:45:00"}}));
    } else if (key == "schedules") {
        rows.append(row({{"科室", "心血管内科诊室"}, {"医生", "张明"}, {"职称", "主任医师"}, {"出诊日期", "2026-05-28"}, {"总号源", 30}, {"剩余号源", 28}, {"状态", 1}}));
        rows.append(row({{"科室", "血液内科诊室"}, {"医生", "周宁"}, {"职称", "主任医师"}, {"出诊日期", "2026-05-28"}, {"总号源", 25}, {"剩余号源", 25}, {"状态", 1}}));
        rows.append(row({{"科室", "普外科诊室"}, {"医生", "李华"}, {"职称", "副主任医师"}, {"出诊日期", "2026-05-28"}, {"总号源", 20}, {"剩余号源", 19}, {"状态", 1}}));
    } else if (key == "doctors") {
        rows.append(row({{"医生姓名", "张明"}, {"所属科室", "心血管内科"}, {"职称", "主任医师"}, {"擅长方向", "高血压、冠心病、心律失常、慢性病管理"}, {"挂号费", 23.00}, {"电话", "13800000003"}, {"状态", 1}}));
        rows.append(row({{"医生姓名", "李华"}, {"所属科室", "普外科"}, {"职称", "副主任医师"}, {"擅长方向", "普外科、创伤处理"}, {"挂号费", 18.00}, {"电话", "13800000004"}, {"状态", 1}}));
        rows.append(row({{"医生姓名", "孙洁"}, {"所属科室", "儿童血液"}, {"职称", "主任医师"}, {"擅长方向", "儿童发热、贫血、血液系统疾病"}, {"挂号费", 22.00}, {"电话", "13800000007"}, {"状态", 1}}));
    } else if (key == "consultations") {
        rows.append(row({{"挂号单号", "R202605280002"}, {"患者", "赵强"}, {"医生", "李华"}, {"主诉", "右下腹疼痛一天"}, {"诊断", "急性腹痛待查"}, {"医嘱", "完善血常规和腹部彩超，清淡饮食，必要时复诊。"}, {"接诊时间", "2026-05-28 10:20:00"}}));
    } else if (key == "prescriptions") {
        rows.append(row({{"处方号", "RX202605280001"}, {"患者", "赵强"}, {"医生", "李华"}, {"状态", "PAID"}, {"处方金额", 36.00}, {"开方时间", "2026-05-28 10:26:00"}}));
    } else if (key == "inventory") {
        rows.append(row({{"药品编码", "D001"}, {"药品名称", "阿莫西林胶囊"}, {"分类", "抗生素"}, {"规格", "0.25g*24粒"}, {"单位", "盒"}, {"售价", 12.00}, {"库存", 120}, {"预警库存", 20}, {"状态", 1}}));
        rows.append(row({{"药品编码", "D002"}, {"药品名称", "布洛芬缓释胶囊"}, {"分类", "解热镇痛"}, {"规格", "0.3g*20粒"}, {"单位", "盒"}, {"售价", 16.00}, {"库存", 80}, {"预警库存", 20}, {"状态", 1}}));
        rows.append(row({{"药品编码", "D003"}, {"药品名称", "奥美拉唑肠溶胶囊"}, {"分类", "消化系统"}, {"规格", "20mg*14粒"}, {"单位", "盒"}, {"售价", 14.00}, {"库存", 60}, {"预警库存", 15}, {"状态", 1}}));
    } else if (key == "bills") {
        rows.append(row({{"账单号", "B202605280001"}, {"患者", "王小兰"}, {"挂号费", 23.00}, {"药品费", 0.00}, {"其他费用", 0.00}, {"合计", 23.00}, {"状态", "UNPAID"}, {"创建时间", "2026-05-28 09:30:00"}}));
        rows.append(row({{"账单号", "B202605280002"}, {"患者", "赵强"}, {"挂号费", 18.00}, {"药品费", 36.00}, {"其他费用", 0.00}, {"合计", 54.00}, {"状态", "PAID"}, {"创建时间", "2026-05-28 10:30:00"}}));
    } else if (key == "statistics") {
        rows.append(row({{"统计日期", "2026-05-28"}, {"科室", "心血管内科诊室"}, {"挂号收入", 23.00}, {"药品收入", 0.00}, {"总收入", 23.00}}));
        rows.append(row({{"统计日期", "2026-05-28"}, {"科室", "普外科诊室"}, {"挂号收入", 18.00}, {"药品收入", 36.00}, {"总收入", 54.00}}));
        rows.append(row({{"统计日期", "2026-05-28"}, {"科室", "全院"}, {"挂号收入", 41.00}, {"药品收入", 36.00}, {"总收入", 77.00}}));
    }

    QJsonObject data;
    data["rows"] = rows;
    data["count"] = rows.size();
    return {true, "Demo data", data};
}

QJsonValue toJsonValue(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QJsonValue();
    }

    if (value.userType() == QMetaType::QDateTime) {
        return value.toDateTime().toString(Qt::ISODate);
    }

    if (value.userType() == QMetaType::QDate) {
        return value.toDate().toString(Qt::ISODate);
    }

    if (value.userType() == QMetaType::QTime) {
        return value.toTime().toString(Qt::ISODate);
    }

    switch (value.userType()) {
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return QString::number(value.toLongLong());
    case QMetaType::Double:
        return value.toDouble();
    default:
        return value.toString();
    }
}

} // namespace

common::Response SqlJson::selectRows(DatabaseManager* database,
                                     const QString& sql,
                                     const QVariantMap& params,
                                     const QString& demoKey)
{
    if (!database->isEnabled()) {
        const auto rows = DemoRepository::instance().rows(demoKey, params.value("keyword").toString());
        QJsonObject data;
        data["rows"] = rows;
        data["count"] = rows.size();
        return {true, "Demo data", data};
    }

    if (!database->ensureOpen()) {
        return {false, "MySQL 连接失败：" + database->lastError(), {}};
    }

    QSqlQuery query(database->database());
    query.prepare(sql);

    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        const QString placeholder = ":" + it.key();
        if (sql.contains(placeholder)) {
            query.bindValue(placeholder, it.value());
        }
    }

    if (!query.exec()) {
        return {false, query.lastError().text(), {}};
    }

    QJsonArray rows;
    const QSqlRecord record = query.record();

    while (query.next()) {
        QJsonObject row;
        for (int i = 0; i < record.count(); ++i) {
            row[record.fieldName(i)] = toJsonValue(query.value(i));
        }
        rows.append(row);
    }
    query.finish();

    QJsonObject data;
    data["rows"] = rows;
    data["count"] = rows.size();
    return {true, "OK", data};
}

} // namespace hospital::server
