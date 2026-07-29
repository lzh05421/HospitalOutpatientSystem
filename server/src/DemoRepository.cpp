#include "server/DemoRepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMutexLocker>
#include <QSaveFile>

#include <algorithm>

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

bool containsKeyword(const QJsonObject& object, const QString& keyword)
{
    if (keyword.trimmed().isEmpty()) {
        return true;
    }

    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().toVariant().toString().contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString patientFieldByName(const QJsonArray& patients, const QString& name, const QString& field)
{
    for (const auto& item : patients) {
        const auto patient = item.toObject();
        if (patient.value("姓名").toString() == name) {
            return patient.value(field).toString();
        }
    }
    return {};
}

QJsonArray sortedScheduleRows(const QJsonArray& rows)
{
    QList<QJsonObject> objects;
    objects.reserve(rows.size());
    for (const auto& item : rows) {
        objects.append(item.toObject());
    }
    std::sort(objects.begin(), objects.end(), [](const QJsonObject& lhs, const QJsonObject& rhs) {
        const QString lhsKey = lhs.value("出诊日期").toString()
            + "|" + lhs.value("科室").toString()
            + "|" + lhs.value("医生").toString();
        const QString rhsKey = rhs.value("出诊日期").toString()
            + "|" + rhs.value("科室").toString()
            + "|" + rhs.value("医生").toString();
        return lhsKey < rhsKey;
    });

    QJsonArray sorted;
    for (const auto& object : objects) {
        sorted.append(object);
    }
    return sorted;
}

QString registrationStatusText(const QString& status)
{
    if (status == "WAITING") return "待叫号";
    if (status == "CALLED") return "已叫号";
    if (status == "IN_CONSULTATION") return "接诊中";
    if (status == "CHECKING") return "检查中";
    if (status == "CHECK_DONE") return "检查完成待复诊";
    if (status == "FINISHED") return "已接诊";
    if (status == "CANCELLED") return "已取消";
    return status;
}

QString prescriptionStatusText(const QString& status)
{
    if (status == "CREATED") return "待审核";
    if (status == "REVIEWED") return "待发药";
    if (status == "DISPENSED") return "已发药";
    if (status == "REJECTED") return "已驳回";
    if (status == "RETURNED") return "已退药";
    if (status == "PAID") return "已缴费";
    if (status == "CANCELLED") return "已取消";
    return status;
}

QString billStatusText(const QString& status)
{
    if (status == "UNPAID") return "待缴费";
    if (status == "PAID") return "已缴费";
    if (status == "REFUNDED") return "已退费";
    if (status == "CANCELLED") return "已取消";
    return status;
}

QString inventoryWarningReason(const QJsonObject& drug)
{
    QStringList reasons;
    if (drug.value("库存").toVariant().toInt() <= drug.value("预警库存").toVariant().toInt()) {
        reasons.append("库存不足");
    }

    const QDate expiry = QDate::fromString(drug.value("有效期").toString(), "yyyy-MM-dd");
    if (expiry.isValid() && expiry < QDate::currentDate()) {
        reasons.append("已过期");
    } else if (expiry.isValid() && expiry <= QDate::currentDate().addDays(30)) {
        reasons.append("近30天到期");
    }
    return reasons.join("；");
}

struct CatalogDoctorSeed
{
    const char* category;
    const char* specialty;
    const char* clinic;
    const char* doctor;
    const char* title;
    double fee;
    const char* phone;
};

const CatalogDoctorSeed catalogDoctorSeeds[] = {
    {"内科门诊", "心血管内科", "心血管内科诊室", "郑凯", "副主任医师", 20.00, "13920000001"},
    {"内科门诊", "心血管内科", "高血压门诊", "王立群", "主任医师", 25.00, "13920000002"},
    {"内科门诊", "心血管内科", "冠心病门诊", "冯若楠", "知名专家（三档）", 30.00, "13920000003"},
    {"内科门诊", "血液内科", "血液内科诊室", "韩亦辰", "知名专家（四档）", 50.00, "13920000004"},
    {"内科门诊", "血液内科", "儿童血液诊室", "许清源", "副主任医师", 20.00, "13920000005"},
    {"内科门诊", "血液内科", "贫血门诊", "沈嘉禾", "主任医师", 25.00, "13920000006"},
    {"内科门诊", "血液内科", "骨髓瘤门诊", "唐雨薇", "知名专家（三档）", 30.00, "13920000007"},
    {"内科门诊", "肾内科", "肾内科诊室", "曹明远", "知名专家（四档）", 50.00, "13920000008"},
    {"内科门诊", "肾内科", "慢性肾病门诊", "梁思远", "副主任医师", 20.00, "13920000009"},
    {"内科门诊", "肾内科", "血液透析门诊", "杜若溪", "主任医师", 25.00, "13920000010"},
    {"内科门诊", "呼吸内科", "呼吸内科诊室", "程浩然", "知名专家（三档）", 30.00, "13920000011"},
    {"内科门诊", "呼吸内科", "哮喘门诊", "叶安琪", "知名专家（四档）", 50.00, "13920000012"},
    {"内科门诊", "呼吸内科", "肺部感染门诊", "薛景行", "副主任医师", 20.00, "13920000013"},
    {"内科门诊", "消化内科", "消化内科诊室", "顾晓曼", "主任医师", 25.00, "13920000014"},
    {"内科门诊", "消化内科", "胃肠门诊", "罗云舟", "知名专家（三档）", 30.00, "13920000015"},
    {"内科门诊", "消化内科", "肝病门诊", "林知远", "知名专家（四档）", 50.00, "13920000016"},
    {"内科门诊", "内分泌科", "内分泌科诊室", "马思齐", "副主任医师", 20.00, "13920000017"},
    {"内科门诊", "内分泌科", "糖尿病门诊", "高子昂", "主任医师", 25.00, "13920000018"},
    {"内科门诊", "内分泌科", "甲状腺门诊", "宋雅宁", "知名专家（三档）", 30.00, "13920000019"},
    {"内科门诊", "神经内科", "神经内科诊室", "何沐阳", "知名专家（四档）", 50.00, "13920000020"},
    {"内科门诊", "神经内科", "头痛门诊", "魏舒然", "副主任医师", 20.00, "13920000021"},
    {"内科门诊", "神经内科", "脑卒中随访门诊", "潘嘉宁", "主任医师", 25.00, "13920000022"},
    {"内科门诊", "风湿免疫科", "风湿免疫科诊室", "方俊逸", "知名专家（三档）", 30.00, "13920000023"},
    {"内科门诊", "风湿免疫科", "类风湿门诊", "袁静姝", "知名专家（四档）", 50.00, "13920000024"},
    {"内科门诊", "风湿免疫科", "痛风门诊", "邹亦凡", "副主任医师", 20.00, "13920000025"},
    {"内科门诊", "老年医学科", "老年医学科诊室", "邵佳怡", "主任医师", 25.00, "13920000026"},
    {"内科门诊", "老年医学科", "老年慢病门诊", "钱思源", "知名专家（三档）", 30.00, "13920000027"},
    {"内科门诊", "老年医学科", "综合评估门诊", "贺明轩", "知名专家（四档）", 50.00, "13920000028"},
    {"外科门诊", "普外科", "普外科诊室", "孟雨桐", "副主任医师", 20.00, "13920000029"},
    {"外科门诊", "普外科", "胃肠外科门诊", "戴清和", "主任医师", 25.00, "13920000030"},
    {"外科门诊", "普外科", "肝胆外科门诊", "陆子涵", "知名专家（三档）", 30.00, "13920000031"},
    {"外科门诊", "骨科", "骨科诊室", "崔明朗", "知名专家（四档）", 50.00, "13920000032"},
    {"外科门诊", "骨科", "关节门诊", "夏以宁", "副主任医师", 20.00, "13920000033"},
    {"外科门诊", "骨科", "脊柱门诊", "钟景澄", "主任医师", 25.00, "13920000034"},
    {"外科门诊", "泌尿外科", "泌尿外科诊室", "姜若谷", "知名专家（三档）", 30.00, "13920000035"},
    {"外科门诊", "泌尿外科", "结石门诊", "白承泽", "知名专家（四档）", 50.00, "13920000036"},
    {"外科门诊", "泌尿外科", "前列腺门诊", "田梓涵", "副主任医师", 20.00, "13920000037"},
    {"外科门诊", "神经外科", "神经外科诊室", "孔令仪", "主任医师", 25.00, "13920000038"},
    {"外科门诊", "神经外科", "颅脑外伤门诊", "严子墨", "知名专家（三档）", 30.00, "13920000039"},
    {"外科门诊", "胸外科", "胸外科诊室", "任清妍", "知名专家（四档）", 50.00, "13920000040"},
    {"外科门诊", "胸外科", "肺结节门诊", "施予安", "副主任医师", 20.00, "13920000041"},
    {"儿科门诊", "儿科普通", "儿科普通诊室", "石文博", "主任医师", 25.00, "13920000042"},
    {"儿科门诊", "儿科普通", "儿童发热门诊", "范若晨", "知名专家（三档）", 30.00, "13920000043"},
    {"儿科门诊", "儿童血液", "儿童血液专病门诊", "乔思淼", "知名专家（四档）", 50.00, "13920000044"},
    {"儿科门诊", "儿童血液", "儿童贫血门诊", "邱逸凡", "副主任医师", 20.00, "13920000045"},
    {"儿科门诊", "儿童呼吸", "儿童呼吸诊室", "洪嘉树", "主任医师", 25.00, "13920000046"},
    {"儿科门诊", "儿童呼吸", "儿童哮喘门诊", "赖雨晴", "知名专家（三档）", 30.00, "13920000047"},
    {"儿科门诊", "儿童消化", "儿童消化诊室", "秦明哲", "知名专家（四档）", 50.00, "13920000048"},
    {"儿科门诊", "儿童消化", "儿童腹痛门诊", "倪语嫣", "副主任医师", 20.00, "13920000049"},
    {"妇产科门诊", "妇科", "妇科诊室", "汤书航", "主任医师", 25.00, "13920000050"},
    {"妇产科门诊", "妇科", "宫颈疾病门诊", "尹静远", "知名专家（三档）", 30.00, "13920000051"},
    {"妇产科门诊", "妇科", "月经病门诊", "常安然", "知名专家（四档）", 50.00, "13920000052"},
    {"妇产科门诊", "产科", "产科诊室", "黎昊天", "副主任医师", 20.00, "13920000053"},
    {"妇产科门诊", "产科", "孕期保健门诊", "莫清秋", "主任医师", 25.00, "13920000054"},
    {"妇产科门诊", "产科", "高危妊娠门诊", "傅子瑜", "知名专家（三档）", 30.00, "13920000055"},
    {"眼耳鼻喉门诊", "眼科", "眼科诊室", "万嘉懿", "知名专家（四档）", 50.00, "13920000056"},
    {"眼耳鼻喉门诊", "眼科", "视光门诊", "江明玥", "副主任医师", 20.00, "13920000057"},
    {"眼耳鼻喉门诊", "眼科", "白内障门诊", "段景然", "主任医师", 25.00, "13920000058"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "耳鼻喉科诊室", "梅若曦", "知名专家（三档）", 30.00, "13920000059"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "鼻炎门诊", "卢泽宇", "知名专家（四档）", 50.00, "13920000060"},
    {"眼耳鼻喉门诊", "耳鼻喉科", "咽喉门诊", "郝星河", "副主任医师", 20.00, "13920000061"},
    {"口腔科门诊", "口腔内科", "口腔内科诊室", "毕语堂", "主任医师", 25.00, "13920000062"},
    {"口腔科门诊", "口腔内科", "牙体牙髓门诊", "康明睿", "知名专家（三档）", 30.00, "13920000063"},
    {"口腔科门诊", "口腔修复科", "口腔修复诊室", "毛思远", "知名专家（四档）", 50.00, "13920000064"},
    {"口腔科门诊", "口腔修复科", "种植修复门诊", "文嘉禾", "副主任医师", 20.00, "13920000065"},
    {"皮肤科门诊", "皮肤科", "皮肤科诊室", "葛雨辰", "主任医师", 25.00, "13920000066"},
    {"皮肤科门诊", "皮肤科", "湿疹门诊", "詹知夏", "知名专家（三档）", 30.00, "13920000067"},
    {"皮肤科门诊", "皮肤科", "痤疮门诊", "樊若云", "知名专家（四档）", 50.00, "13920000068"},
    {"中医科门诊", "中医内科", "中医内科诊室", "纪明扬", "副主任医师", 20.00, "13920000069"},
    {"中医科门诊", "中医内科", "失眠调理门诊", "温清越", "主任医师", 25.00, "13920000070"},
    {"中医科门诊", "中医内科", "针灸门诊", "龙启航", "知名专家（三档）", 30.00, "13920000071"},
    {"中医科门诊", "中医妇科", "中医妇科诊室", "向思衡", "知名专家（四档）", 50.00, "13920000072"},
    {"中医科门诊", "中医妇科", "月经调理门诊", "苗若琳", "副主任医师", 20.00, "13920000073"},
    {"中医科门诊", "康复理疗科", "康复理疗诊室", "申明远", "主任医师", 25.00, "13920000074"},
    {"中医科门诊", "康复理疗科", "颈肩腰腿痛门诊", "欧阳静安", "知名专家（三档）", 30.00, "13920000075"}
};

} // namespace

DemoRepository& DemoRepository::instance()
{
    static DemoRepository repository;
    return repository;
}

DemoRepository::DemoRepository()
{
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    const QString yesterday = QDate::currentDate().addDays(-1).toString("yyyy-MM-dd");
    const QString tomorrow = QDate::currentDate().addDays(1).toString("yyyy-MM-dd");
    const QString nextWeek = QDate::currentDate().addDays(7).toString("yyyy-MM-dd");

    m_patients = {
        row({{"患者编号", "P20260001"}, {"姓名", "王小兰"}, {"性别", "女"}, {"电话", "13910000001"}, {"身份证号", "110101199603180021"}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 3}, {"最近就诊", today + " 08:30:00"}, {"地址", "北京市海淀区知春路18号"}, {"建档时间", "2026-05-20 09:10:00"}}),
        row({{"患者编号", "P20260002"}, {"姓名", "赵强"}, {"性别", "男"}, {"电话", "13910000002"}, {"身份证号", "110101198811020033"}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 2}, {"最近就诊", today + " 09:45:00"}, {"地址", "北京市朝阳区望京西园"}, {"建档时间", "2026-05-21 09:25:00"}}),
        row({{"患者编号", "P20260003"}, {"姓名", "陈晨"}, {"性别", "女"}, {"电话", "13910000003"}, {"身份证号", "110101201807090044"}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 1}, {"最近就诊", today + " 10:05:00"}, {"地址", "北京市西城区"}, {"建档时间", "2026-05-22 10:05:00"}}),
        row({{"患者编号", "P20260004"}, {"姓名", "李建国"}, {"性别", "男"}, {"电话", "13910000004"}, {"身份证号", "110101197205060055"}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 1}, {"最近就诊", today + " 08:45:00"}, {"地址", "北京市丰台区"}, {"建档时间", "2026-05-25 08:40:00"}}),
        row({{"患者编号", "P20260005"}, {"姓名", "刘芳"}, {"性别", "女"}, {"电话", "13910000005"}, {"身份证号", "110101199112120066"}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 1}, {"最近就诊", today + " 09:10:00"}, {"地址", "北京市通州区"}, {"建档时间", "2026-05-26 09:00:00"}}),
        row({{"患者编号", "P20260006"}, {"姓名", "孙浩"}, {"性别", "男"}, {"电话", ""}, {"身份证号", "110101200102030077"}, {"身份登记", "已登记"}, {"患者状态", "仅建档"}, {"就诊次数", 0}, {"最近就诊", ""}, {"地址", ""}, {"建档时间", "2026-05-30 11:10:00"}}),
        row({{"患者编号", "P20260007"}, {"姓名", "周雨桐"}, {"性别", "女"}, {"电话", "13910000007"}, {"身份证号", ""}, {"身份登记", "已登记"}, {"患者状态", "已确认患者"}, {"就诊次数", 1}, {"最近就诊", today + " 10:20:00"}, {"地址", "北京市昌平区"}, {"建档时间", "2026-06-01 10:00:00"}}),
        row({{"患者编号", "P20260008"}, {"姓名", "吴一鸣"}, {"性别", "男"}, {"电话", "13910000008"}, {"身份证号", "110101201509010088"}, {"身份登记", "已登记"}, {"患者状态", "仅建档"}, {"就诊次数", 0}, {"最近就诊", ""}, {"地址", "北京市东城区"}, {"建档时间", "2026-06-02 14:05:00"}})
    };

    m_departments = {
        row({{"科室编码", "DEP001"}, {"科室名称", "内科门诊"}, {"位置", "门诊楼二层"}, {"状态", 1}}),
        row({{"科室编码", "DEP00101"}, {"科室名称", "心血管内科"}, {"位置", "门诊楼二层A区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010101"}, {"科室名称", "心血管内科诊室"}, {"位置", "门诊楼二层A区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010102"}, {"科室名称", "高血压门诊"}, {"位置", "门诊楼二层A区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010103"}, {"科室名称", "冠心病门诊"}, {"位置", "门诊楼二层A区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00102"}, {"科室名称", "血液内科"}, {"位置", "门诊楼二层B区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010201"}, {"科室名称", "血液内科诊室"}, {"位置", "门诊楼二层B区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010202"}, {"科室名称", "儿童血液诊室"}, {"位置", "门诊楼二层B区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010203"}, {"科室名称", "贫血门诊"}, {"位置", "门诊楼二层B区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010204"}, {"科室名称", "骨髓瘤门诊"}, {"位置", "门诊楼二层B区4诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00103"}, {"科室名称", "肾内科"}, {"位置", "门诊楼二层C区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010301"}, {"科室名称", "肾内科诊室"}, {"位置", "门诊楼二层C区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010302"}, {"科室名称", "慢性肾病门诊"}, {"位置", "门诊楼二层C区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010303"}, {"科室名称", "血液透析门诊"}, {"位置", "门诊楼二层C区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00104"}, {"科室名称", "呼吸内科"}, {"位置", "门诊楼二层D区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010401"}, {"科室名称", "呼吸内科诊室"}, {"位置", "门诊楼二层D区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010402"}, {"科室名称", "哮喘门诊"}, {"位置", "门诊楼二层D区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010403"}, {"科室名称", "肺部感染门诊"}, {"位置", "门诊楼二层D区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00105"}, {"科室名称", "消化内科"}, {"位置", "门诊楼二层E区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010501"}, {"科室名称", "消化内科诊室"}, {"位置", "门诊楼二层E区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010502"}, {"科室名称", "胃肠门诊"}, {"位置", "门诊楼二层E区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010503"}, {"科室名称", "肝病门诊"}, {"位置", "门诊楼二层E区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00106"}, {"科室名称", "内分泌科"}, {"位置", "门诊楼二层F区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010601"}, {"科室名称", "内分泌科诊室"}, {"位置", "门诊楼二层F区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010602"}, {"科室名称", "糖尿病门诊"}, {"位置", "门诊楼二层F区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0010603"}, {"科室名称", "甲状腺门诊"}, {"位置", "门诊楼二层F区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP002"}, {"科室名称", "外科门诊"}, {"位置", "门诊楼三层"}, {"状态", 1}}),
        row({{"科室编码", "DEP00201"}, {"科室名称", "普外科"}, {"位置", "门诊楼三层A区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020101"}, {"科室名称", "普外科诊室"}, {"位置", "门诊楼三层A区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020102"}, {"科室名称", "胃肠外科门诊"}, {"位置", "门诊楼三层A区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020103"}, {"科室名称", "肝胆外科门诊"}, {"位置", "门诊楼三层A区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00202"}, {"科室名称", "骨科"}, {"位置", "门诊楼三层B区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020201"}, {"科室名称", "骨科诊室"}, {"位置", "门诊楼三层B区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020202"}, {"科室名称", "关节门诊"}, {"位置", "门诊楼三层B区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0020203"}, {"科室名称", "脊柱门诊"}, {"位置", "门诊楼三层B区3诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP003"}, {"科室名称", "儿科门诊"}, {"位置", "门诊楼一层"}, {"状态", 1}}),
        row({{"科室编码", "DEP00301"}, {"科室名称", "儿科普通"}, {"位置", "门诊楼一层A区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0030101"}, {"科室名称", "儿科普通诊室"}, {"位置", "门诊楼一层A区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0030102"}, {"科室名称", "儿童发热门诊"}, {"位置", "门诊楼一层A区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP00302"}, {"科室名称", "儿童血液"}, {"位置", "门诊楼一层B区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0030201"}, {"科室名称", "儿童血液专病门诊"}, {"位置", "门诊楼一层B区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0030202"}, {"科室名称", "儿童贫血门诊"}, {"位置", "门诊楼一层B区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP004"}, {"科室名称", "中医科门诊"}, {"位置", "门诊楼四层"}, {"状态", 1}}),
        row({{"科室编码", "DEP00401"}, {"科室名称", "中医内科"}, {"位置", "门诊楼四层A区"}, {"状态", 1}}),
        row({{"科室编码", "DEP0040101"}, {"科室名称", "中医内科诊室"}, {"位置", "门诊楼四层A区1诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0040102"}, {"科室名称", "失眠调理门诊"}, {"位置", "门诊楼四层A区2诊室"}, {"状态", 1}}),
        row({{"科室编码", "DEP0040103"}, {"科室名称", "针灸门诊"}, {"位置", "门诊楼四层A区3诊室"}, {"状态", 1}})
    };

    m_registrations = {
        row({{"挂号单号", "R202606050001"}, {"患者", "王小兰"}, {"科室", "心血管内科诊室"}, {"医生", "张明"}, {"就诊日期", today}, {"时段", "08:30-09:00"}, {"状态", "WAITING"}, {"挂号费", 23.00}, {"挂号时间", today + " 08:30:00"}}),
        row({{"挂号单号", "R202606050002"}, {"患者", "赵强"}, {"科室", "普外科诊室"}, {"医生", "李华"}, {"就诊日期", today}, {"时段", "09:00-09:30"}, {"状态", "FINISHED"}, {"挂号费", 18.00}, {"挂号时间", today + " 09:00:00"}}),
        row({{"挂号单号", "R202606050003"}, {"患者", "陈晨"}, {"科室", "儿童血液专病门诊"}, {"医生", "孙洁"}, {"就诊日期", tomorrow}, {"时段", "09:30-10:00"}, {"状态", "WAITING"}, {"挂号费", 22.00}, {"挂号时间", today + " 09:18:00"}}),
        row({{"挂号单号", "R202606050004"}, {"患者", "李建国"}, {"科室", "呼吸内科诊室"}, {"医生", "陈晓"}, {"就诊日期", today}, {"时段", "10:00-10:30"}, {"状态", "CALLED"}, {"挂号费", 20.00}, {"挂号时间", today + " 08:45:00"}}),
        row({{"挂号单号", "R202606050005"}, {"患者", "刘芳"}, {"科室", "血液内科诊室"}, {"医生", "周宁"}, {"就诊日期", today}, {"时段", "10:30-11:00"}, {"状态", "CHECK_DONE"}, {"挂号费", 23.00}, {"挂号时间", today + " 09:10:00"}}),
        row({{"挂号单号", "R202606050006"}, {"患者", "周雨桐"}, {"科室", "肾内科诊室"}, {"医生", "刘洋"}, {"就诊日期", today}, {"时段", "11:00-11:30"}, {"状态", "CANCELLED"}, {"挂号费", 20.00}, {"挂号时间", today + " 10:20:00"}}),
        row({{"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"科室", "心血管内科诊室"}, {"医生", "张明"}, {"就诊日期", yesterday}, {"时段", "14:00-14:30"}, {"状态", "FINISHED"}, {"挂号费", 23.00}, {"挂号时间", yesterday + " 13:40:00"}})
    };

    m_schedules = {
        row({{"科室", "心血管内科诊室"}, {"医生", "张明"}, {"职称", "主任医师"}, {"出诊日期", yesterday}, {"总号源", 30}, {"剩余号源", 29}, {"状态", 1}}),
        row({{"科室", "心血管内科诊室"}, {"医生", "张明"}, {"职称", "主任医师"}, {"出诊日期", today}, {"总号源", 30}, {"剩余号源", 28}, {"状态", 1}}),
        row({{"科室", "心血管内科诊室"}, {"医生", "张明"}, {"职称", "主任医师"}, {"出诊日期", tomorrow}, {"总号源", 30}, {"剩余号源", 30}, {"状态", 1}}),
        row({{"科室", "血液内科诊室"}, {"医生", "周宁"}, {"职称", "主任医师"}, {"出诊日期", today}, {"总号源", 25}, {"剩余号源", 24}, {"状态", 1}}),
        row({{"科室", "肾内科诊室"}, {"医生", "刘洋"}, {"职称", "副主任医师"}, {"出诊日期", today}, {"总号源", 25}, {"剩余号源", 24}, {"状态", 1}}),
        row({{"科室", "呼吸内科诊室"}, {"医生", "陈晓"}, {"职称", "副主任医师"}, {"出诊日期", today}, {"总号源", 25}, {"剩余号源", 24}, {"状态", 1}}),
        row({{"科室", "普外科诊室"}, {"医生", "李华"}, {"职称", "副主任医师"}, {"出诊日期", today}, {"总号源", 30}, {"剩余号源", 29}, {"状态", 1}}),
        row({{"科室", "普外科诊室"}, {"医生", "李华"}, {"职称", "副主任医师"}, {"出诊日期", tomorrow}, {"总号源", 20}, {"剩余号源", 20}, {"状态", 1}}),
        row({{"科室", "儿童血液专病门诊"}, {"医生", "孙洁"}, {"职称", "主任医师"}, {"出诊日期", tomorrow}, {"总号源", 20}, {"剩余号源", 19}, {"状态", 1}}),
        row({{"科室", "肾内科诊室"}, {"医生", "刘洋"}, {"职称", "副主任医师"}, {"出诊日期", nextWeek}, {"总号源", 25}, {"剩余号源", 25}, {"状态", 1}})
    };

    m_doctors = {
        row({{"医生姓名", "张明"}, {"所属科室", "心血管内科"}, {"职称", "主任医师"}, {"擅长方向", "高血压、冠心病、心律失常、慢性病管理"}, {"挂号费", 23.00}, {"电话", "13800000003"}, {"状态", 1}}),
        row({{"医生姓名", "周宁"}, {"所属科室", "血液内科"}, {"职称", "主任医师"}, {"擅长方向", "贫血、白细胞异常、血小板减少、淋巴结肿大"}, {"挂号费", 23.00}, {"电话", "13800000008"}, {"状态", 1}}),
        row({{"医生姓名", "刘洋"}, {"所属科室", "肾内科"}, {"职称", "副主任医师"}, {"擅长方向", "慢性肾病、蛋白尿、血液透析随访"}, {"挂号费", 20.00}, {"电话", "13800000011"}, {"状态", 1}}),
        row({{"医生姓名", "陈晓"}, {"所属科室", "呼吸内科"}, {"职称", "副主任医师"}, {"擅长方向", "咳嗽、哮喘、慢阻肺、肺部感染"}, {"挂号费", 20.00}, {"电话", "13800000009"}, {"状态", 1}}),
        row({{"医生姓名", "李华"}, {"所属科室", "普外科"}, {"职称", "副主任医师"}, {"擅长方向", "普外科、创伤处理、腹痛待查"}, {"挂号费", 18.00}, {"电话", "13800000004"}, {"状态", 1}}),
        row({{"医生姓名", "孙洁"}, {"所属科室", "儿童血液"}, {"职称", "主任医师"}, {"擅长方向", "儿童发热、贫血、血液系统疾病"}, {"挂号费", 22.00}, {"电话", "13800000007"}, {"状态", 1}})
    };

    m_consultations = {
        row({{"挂号单号", "R202606050002"}, {"患者", "赵强"}, {"医生", "李华"}, {"主诉", "右下腹疼痛一天"}, {"现病史", "疼痛位于右下腹，活动后加重，伴轻微恶心，无明显呕吐。"}, {"既往史", "否认慢性病史及药物过敏史。"}, {"体格检查", "右下腹压痛，反跳痛不明显，体温37.5℃。"}, {"ICD编码", "R10.401"}, {"诊断", "急性腹痛待查"}, {"医嘱", "完善血常规和腹部彩超，清淡饮食，必要时复诊。"}, {"外院报告医院", "市人民医院"}, {"外院报告类型", "超声"}, {"外院报告日期", today}, {"外院报告摘要", "外院腹部超声提示阑尾区未见明确包块。"}, {"外院报告结论", "建议结合临床及血常规复查。"}, {"外院报告附件", ""}, {"接诊时间", today + " 09:45:00"}}),
        row({{"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"医生", "张明"}, {"主诉", "反复胸闷一周"}, {"现病史", "近一周反复胸闷，活动后明显，休息后可缓解。"}, {"既往史", "既往血压偏高，规律用药情况一般。"}, {"体格检查", "血压145/92mmHg，心肺听诊未闻及明显异常。"}, {"ICD编码", "I10.x00"}, {"诊断", "高血压病，冠心病待排"}, {"医嘱", "低盐饮食，规律监测血压，一周后复诊。"}, {"外院报告医院", "社区卫生服务中心"}, {"外院报告类型", "心电图"}, {"外院报告日期", yesterday}, {"外院报告摘要", "外院心电图提示窦性心律。"}, {"外院报告结论", "ST-T轻度改变，建议专科进一步评估。"}, {"外院报告附件", ""}, {"接诊时间", yesterday + " 14:25:00"}})
    };

    m_prescriptions = {
        row({{"处方号", "RX202606050001"}, {"挂号单号", "R202606050002"}, {"患者", "赵强"}, {"医生", "李华"}, {"状态", "PAID"}, {"药品明细", "阿莫西林胶囊 x2；奥美拉唑肠溶胶囊 x1"}, {"药品名称", "阿莫西林胶囊"}, {"数量", 2}, {"处方金额", 38.00}, {"开方时间", today + " 09:50:00"}}),
        row({{"处方号", "RX202606050002"}, {"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"医生", "张明"}, {"状态", "待审核"}, {"药品明细", "布洛芬缓释胶囊 x1"}, {"药品名称", "布洛芬缓释胶囊"}, {"数量", 1}, {"处方金额", 16.00}, {"开方时间", today + " 10:05:00"}}),
        row({{"处方号", "RX202606050003"}, {"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"医生", "张明"}, {"状态", "待发药"}, {"药品明细", "硝苯地平缓释片 x2"}, {"药品名称", "硝苯地平缓释片"}, {"数量", 2}, {"处方金额", 44.00}, {"审核人", "药房管理员"}, {"审核时间", today + " 10:12:00"}, {"开方时间", today + " 10:08:00"}}),
        row({{"处方号", "RX202606050004"}, {"挂号单号", "R202606050002"}, {"患者", "赵强"}, {"医生", "李华"}, {"状态", "已发药"}, {"药品明细", "蒙脱石散 x1"}, {"药品名称", "蒙脱石散"}, {"数量", 1}, {"处方金额", 18.00}, {"发药人", "药房管理员"}, {"发药时间", today + " 10:22:00"}, {"开方时间", today + " 10:15:00"}})
    };

    m_examinations = {
        row({{"检查单号", "EX202606050001"}, {"挂号单号", "R202606050005"}, {"患者", "刘芳"}, {"医生", "周宁"}, {"检查项目", "血常规"}, {"申请说明", "贫血原因筛查"}, {"检查结果", ""}, {"状态", "待检查"}, {"申请时间", today + " 09:30:00"}, {"完成时间", ""}}),
        row({{"检查单号", "EX202606050002"}, {"挂号单号", "R202606050004"}, {"患者", "李建国"}, {"医生", "陈晓"}, {"检查项目", "胸部DR"}, {"申请说明", "咳嗽伴胸闷"}, {"检查结果", "双肺纹理增多，未见明显实变影。"}, {"状态", "已完成"}, {"申请时间", today + " 09:05:00"}, {"完成时间", today + " 09:40:00"}}),
        row({{"检查单号", "EX202606040001"}, {"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"医生", "张明"}, {"检查项目", "心电图"}, {"申请说明", "胸闷心悸评估"}, {"检查结果", "窦性心律，ST-T轻度改变。"}, {"状态", "已完成"}, {"申请时间", yesterday + " 14:05:00"}, {"完成时间", yesterday + " 14:18:00"}})
    };

    m_inventory = {
        row({{"药品编码", "D001"}, {"条形码", "6900000000011"}, {"药品名称", "阿莫西林胶囊"}, {"分类", "抗生素"}, {"规格", "0.25g*24粒"}, {"单位", "盒"}, {"售价", 12.00}, {"库存", 120}, {"预警库存", 20}, {"有效期", QDate::currentDate().addYears(1).toString("yyyy-MM-dd")}, {"预警原因", ""}, {"状态", 1}}),
        row({{"药品编码", "D002"}, {"条形码", "6900000000028"}, {"药品名称", "布洛芬缓释胶囊"}, {"分类", "解热镇痛"}, {"规格", "0.3g*20粒"}, {"单位", "盒"}, {"售价", 16.00}, {"库存", 80}, {"预警库存", 20}, {"有效期", QDate::currentDate().addDays(20).toString("yyyy-MM-dd")}, {"预警原因", "近30天到期"}, {"状态", 1}}),
        row({{"药品编码", "D003"}, {"条形码", "6900000000035"}, {"药品名称", "奥美拉唑肠溶胶囊"}, {"分类", "消化系统"}, {"规格", "20mg*14粒"}, {"单位", "盒"}, {"售价", 14.00}, {"库存", 8}, {"预警库存", 15}, {"有效期", QDate::currentDate().addYears(1).toString("yyyy-MM-dd")}, {"预警原因", "库存不足"}, {"状态", 1}}),
        row({{"药品编码", "D004"}, {"条形码", "6900000000042"}, {"药品名称", "硝苯地平缓释片"}, {"分类", "心血管"}, {"规格", "20mg*30片"}, {"单位", "盒"}, {"售价", 22.00}, {"库存", 45}, {"预警库存", 20}, {"有效期", QDate::currentDate().addMonths(8).toString("yyyy-MM-dd")}, {"预警原因", ""}, {"状态", 1}}),
        row({{"药品编码", "D005"}, {"条形码", "6900000000059"}, {"药品名称", "蒙脱石散"}, {"分类", "消化系统"}, {"规格", "3g*10袋"}, {"单位", "盒"}, {"售价", 18.00}, {"库存", 5}, {"预警库存", 10}, {"有效期", QDate::currentDate().addDays(12).toString("yyyy-MM-dd")}, {"预警原因", "库存不足；近30天到期"}, {"状态", 1}}),
        row({{"药品编码", "D006"}, {"条形码", "6900000000066"}, {"药品名称", "葡萄糖酸亚铁片"}, {"分类", "血液系统"}, {"规格", "0.3g*60片"}, {"单位", "瓶"}, {"售价", 26.00}, {"库存", 33}, {"预警库存", 12}, {"有效期", QDate::currentDate().addYears(1).toString("yyyy-MM-dd")}, {"预警原因", ""}, {"状态", 1}})
    };

    m_bills = {
        row({{"账单号", "B202606050001"}, {"挂号单号", "R202606050001"}, {"患者", "王小兰"}, {"挂号费", 23.00}, {"药品费", 0.00}, {"其他费用", 0.00}, {"合计", 23.00}, {"状态", "UNPAID"}, {"创建时间", today + " 08:30:00"}}),
        row({{"账单号", "B202606050002"}, {"挂号单号", "R202606050002"}, {"患者", "赵强"}, {"挂号费", 18.00}, {"药品费", 56.00}, {"其他费用", 30.00}, {"合计", 104.00}, {"状态", "PAID"}, {"支付方式", "微信支付"}, {"发票号", "PAY202606050001"}, {"支付时间", today + " 10:35:00"}, {"创建时间", today + " 09:00:00"}}),
        row({{"账单号", "B202606050003"}, {"挂号单号", "R202606050003"}, {"患者", "陈晨"}, {"挂号费", 22.00}, {"药品费", 0.00}, {"其他费用", 0.00}, {"合计", 22.00}, {"状态", "UNPAID"}, {"创建时间", today + " 09:18:00"}}),
        row({{"账单号", "B202606050004"}, {"挂号单号", "R202606050004"}, {"患者", "李建国"}, {"挂号费", 20.00}, {"药品费", 0.00}, {"其他费用", 45.00}, {"合计", 65.00}, {"状态", "PAID"}, {"支付方式", "现金"}, {"发票号", "PAY202606050002"}, {"支付时间", today + " 09:42:00"}, {"创建时间", today + " 08:45:00"}}),
        row({{"账单号", "B202606050005"}, {"挂号单号", "R202606050005"}, {"患者", "刘芳"}, {"挂号费", 23.00}, {"药品费", 0.00}, {"其他费用", 25.00}, {"合计", 48.00}, {"状态", "UNPAID"}, {"创建时间", today + " 09:10:00"}}),
        row({{"账单号", "B202606050006"}, {"挂号单号", "R202606050006"}, {"患者", "周雨桐"}, {"挂号费", 20.00}, {"药品费", 0.00}, {"其他费用", 0.00}, {"合计", 20.00}, {"状态", "CANCELLED"}, {"创建时间", today + " 10:20:00"}}),
        row({{"账单号", "B202606040001"}, {"挂号单号", "R202606040001"}, {"患者", "王小兰"}, {"挂号费", 23.00}, {"药品费", 44.00}, {"其他费用", 20.00}, {"合计", 87.00}, {"状态", "REFUNDED"}, {"支付方式", "支付宝"}, {"发票号", "PAY202606040001"}, {"支付时间", yesterday + " 15:00:00"}, {"退费原因", "患者申请退费"}, {"退费时间", today + " 08:05:00"}, {"创建时间", yesterday + " 13:40:00"}})
    };

    m_statistics = {
        row({{"统计日期", yesterday}, {"科室", "心血管内科"}, {"挂号收入", 23.00}, {"药品收入", 44.00}, {"总收入", 87.00}}),
        row({{"统计日期", today}, {"科室", "心血管内科"}, {"挂号收入", 23.00}, {"药品收入", 0.00}, {"总收入", 23.00}}),
        row({{"统计日期", today}, {"科室", "普外科"}, {"挂号收入", 18.00}, {"药品收入", 56.00}, {"总收入", 104.00}}),
        row({{"统计日期", today}, {"科室", "呼吸内科"}, {"挂号收入", 20.00}, {"药品收入", 0.00}, {"总收入", 65.00}}),
        row({{"统计日期", today}, {"科室", "血液内科"}, {"挂号收入", 23.00}, {"药品收入", 0.00}, {"总收入", 48.00}}),
        row({{"统计日期", today}, {"科室", "全院"}, {"挂号收入", 84.00}, {"药品收入", 56.00}, {"总收入", 240.00}}),
        row({{"统计日期", tomorrow}, {"科室", "儿童血液"}, {"挂号收入", 22.00}, {"药品收入", 0.00}, {"总收入", 22.00}})
    };

    m_operationLogs = {
        row({{"操作人", "挂号员一号"}, {"模块", "挂号管理"}, {"动作", "新增挂号"}, {"内容", "为王小兰创建今日心血管内科预约，时段 08:30-09:00。"}, {"操作时间", today + " 08:30:00"}}),
        row({{"操作人", "张明"}, {"模块", "医生排班"}, {"动作", "重新排班"}, {"内容", "清空旧号源并生成全天排班。"}, {"操作时间", today + " 08:35:00"}}),
        row({{"操作人", "陈晓"}, {"模块", "医生接诊"}, {"动作", "叫号"}, {"内容", "呼叫李建国进入呼吸内科诊室。"}, {"操作时间", today + " 09:00:00"}}),
        row({{"操作人", "周宁"}, {"模块", "检查管理"}, {"动作", "开立检查"}, {"内容", "为刘芳开立血常规检查。"}, {"操作时间", today + " 09:30:00"}}),
        row({{"操作人", "药房管理员"}, {"模块", "处方管理"}, {"动作", "审核处方"}, {"内容", "审核通过 RX202606050003。"}, {"操作时间", today + " 10:12:00"}}),
        row({{"操作人", "收费员一号"}, {"模块", "收费管理"}, {"动作", "收费"}, {"内容", "赵强账单 B202606050002 已完成微信支付。"}, {"操作时间", today + " 10:35:00"}}),
        row({{"操作人", "系统管理员"}, {"模块", "患者管理"}, {"动作", "导出"}, {"内容", "导出患者管理 CSV 测试数据。"}, {"操作时间", today + " 10:50:00"}})
    };

    loadState();
    ensureCatalogData();
}

QJsonArray DemoRepository::rows(const QString& key, const QString& keyword)
{
    QMutexLocker locker(&m_mutex);
    const QHash<QString, QJsonArray*> tables = {
        {"patients", &m_patients},
        {"departments", &m_departments},
        {"registrations", &m_registrations},
        {"schedules", &m_schedules},
        {"doctors", &m_doctors},
        {"consultations", &m_consultations},
        {"prescriptions", &m_prescriptions},
        {"examinations", &m_examinations},
        {"inventory", &m_inventory},
        {"bills", &m_bills},
        {"statistics", &m_statistics},
        {"operationLogs", &m_operationLogs}
    };

    QJsonArray result;
    auto* source = tables.value(key, nullptr);
    if (!source) {
        return result;
    }

    for (const auto& item : *source) {
        auto object = item.toObject();
        const int status = object.contains("状态") ? object.value("状态").toVariant().toInt() : 1;
        if ((key == "departments" || key == "doctors" || key == "schedules" || key == "inventory")
            && status == 0) {
            continue;
        }
        if (key == "patients") {
            const bool hasPhone = !object.value("电话").toString().trimmed().isEmpty();
            const bool hasIdCard = !object.value("身份证号").toString().trimmed().isEmpty();
            const bool hasAddress = !object.value("地址").toString().trimmed().isEmpty();
            const int visits = object.value("就诊次数").toVariant().toInt();
            const int completeness = (hasPhone ? 25 : 0)
                + (hasIdCard ? 25 : 0)
                + (hasAddress ? 20 : 0)
                + (visits > 0 ? 30 : 0);
            object["档案完整度"] = QString::number(completeness) + "%";
            if (visits >= 2) {
                object["智能提示"] = "复诊患者";
            } else if (!hasPhone || !hasIdCard) {
                object["智能提示"] = "资料待补全";
            } else if (visits == 1) {
                object["智能提示"] = "首次就诊已确认";
            } else {
                object["智能提示"] = "新建档案";
            }
        }
        if (key == "schedules") {
            object.remove("时段");
        }
        if (object.contains("患者") && !object.contains("身份证号")) {
            object["身份证号"] = patientFieldByName(m_patients, object.value("患者").toString(), "身份证号");
        }
        if (key == "statistics") {
            const double total = object.value("总收入").toVariant().toDouble();
            const double drug = object.value("药品收入").toVariant().toDouble();
            object["药品占比"] = total > 0
                ? QString::number(drug / total * 100.0, 'f', 1) + "%"
                : QString("0%");
        }
        if (containsKeyword(object, keyword)) {
            result.append(object);
        }
    }
    if (key == "schedules") {
        return sortedScheduleRows(result);
    }
    return result;
}

QJsonArray DemoRepository::patientRecords(const QString& keyword)
{
    QMutexLocker locker(&m_mutex);
    QJsonArray result;
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        QJsonObject row;
        row["患者"] = registration.value("患者").toString();
        row["挂号单号"] = registration.value("挂号单号").toString();
        row["科室"] = registration.value("科室").toString();
        row["医生"] = registration.value("医生").toString();
        row["就诊日期"] = registration.value("就诊日期").toString();
        row["挂号状态"] = registrationStatusText(registration.value("状态").toString());
        row["挂号费"] = registration.value("挂号费").toVariant().toDouble();
        row["主诉"] = "";
        row["现病史"] = "";
        row["既往史"] = "";
        row["体格检查"] = "";
        row["ICD编码"] = "";
        row["诊断"] = "";
        row["医嘱"] = "";
        row["外院报告医院"] = "";
        row["外院报告类型"] = "";
        row["外院报告日期"] = "";
        row["外院报告摘要"] = "";
        row["外院报告结论"] = "";
        row["外院报告附件"] = "";
        row["处方金额"] = 0.0;
        row["账单状态"] = "";

        for (const auto& patientItem : m_patients) {
            const auto patient = patientItem.toObject();
            if (patient.value("姓名").toString() == row.value("患者").toString()) {
                row["患者编号"] = patient.value("患者编号").toString();
                row["电话"] = patient.value("电话").toString();
                row["身份证号"] = patient.value("身份证号").toString();
                break;
            }
        }
        for (const auto& recordItem : m_consultations) {
            const auto record = recordItem.toObject();
            if (record.value("挂号单号").toString() == row.value("挂号单号").toString()) {
                row["主诉"] = record.value("主诉").toString();
                row["现病史"] = record.value("现病史").toString();
                row["既往史"] = record.value("既往史").toString();
                row["体格检查"] = record.value("体格检查").toString();
                row["ICD编码"] = record.value("ICD编码").toString();
                row["诊断"] = record.value("诊断").toString();
                row["医嘱"] = record.value("医嘱").toString();
                row["外院报告医院"] = record.value("外院报告医院").toString();
                row["外院报告类型"] = record.value("外院报告类型").toString();
                row["外院报告日期"] = record.value("外院报告日期").toString();
                row["外院报告摘要"] = record.value("外院报告摘要").toString();
                row["外院报告结论"] = record.value("外院报告结论").toString();
                row["外院报告附件"] = record.value("外院报告附件").toString();
                row["接诊时间"] = record.value("接诊时间").toString();
                break;
            }
        }
        for (const auto& prescriptionItem : m_prescriptions) {
            const auto prescription = prescriptionItem.toObject();
            if (prescription.value("挂号单号").toString() == row.value("挂号单号").toString()) {
                row["处方号"] = prescription.value("处方号").toString();
                row["处方状态"] = prescriptionStatusText(prescription.value("状态").toString());
                row["处方金额"] = prescription.value("处方金额").toVariant().toDouble();
                break;
            }
        }
        for (const auto& billItem : m_bills) {
            const auto bill = billItem.toObject();
            if (bill.value("挂号单号").toString() == row.value("挂号单号").toString()
                || (!bill.contains("挂号单号") && bill.value("患者").toString() == row.value("患者").toString())) {
                row["账单号"] = bill.value("账单号").toString();
                row["账单状态"] = billStatusText(bill.value("状态").toString());
                row["费用合计"] = bill.value("合计").toVariant().toDouble();
                break;
            }
        }
        if (containsKeyword(row, keyword)) {
            result.append(row);
        }
    }
    return result;
}

QJsonArray DemoRepository::waitingQueue(const QString& keyword,
                                        const QString& departmentFilter,
                                        const QString& doctorFilter,
                                        const QString& clinicTypeFilter)
{
    QMutexLocker locker(&m_mutex);
    QJsonArray result;
    QHash<QString, int> queueCounts;
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        const QString status = registration.value("状态").toString();
        if (status != "WAITING" && status != "CALLED" && status != "CHECK_DONE") {
            continue;
        }

        QString title;
        double fee = 0.0;
        for (const auto& doctorItem : m_doctors) {
            const auto doctor = doctorItem.toObject();
            if (doctor.value("医生姓名").toString() == registration.value("医生").toString()) {
                title = doctor.value("职称").toString();
                fee = doctor.value("挂号费").toVariant().toDouble();
                break;
            }
        }

        QJsonObject row;
        row["科室"] = registration.value("科室").toString();
        row["医生"] = registration.value("医生").toString();
        row["职称"] = title;
        row["号别"] = title.contains("主任") || fee >= 30.0 ? "专家号" : "普通号";
        row["患者"] = registration.value("患者").toString();
        row["身份证号"] = patientFieldByName(m_patients, row.value("患者").toString(), "身份证号");
        row["挂号单号"] = registration.value("挂号单号").toString();
        row["就诊日期"] = registration.value("就诊日期").toString();
        row["时段"] = registration.value("时段").toString();
        row["候诊状态"] = registrationStatusText(status);
        const QString queueKey = row.value("医生").toString() + "|" + row.value("就诊日期").toString();
        const int queueNo = ++queueCounts[queueKey];
        row["排队序号"] = queueNo;
        row["预计等待"] = QString::number(qMax(0, queueNo - 1) * 8) + "分钟";
        row["挂号时间"] = registration.value("挂号时间").toString();
        if (!departmentFilter.isEmpty() && row.value("科室").toString() != departmentFilter) {
            continue;
        }
        if (!doctorFilter.isEmpty() && row.value("医生").toString() != doctorFilter) {
            continue;
        }
        if (!clinicTypeFilter.isEmpty() && row.value("号别").toString() != clinicTypeFilter) {
            continue;
        }
        if (containsKeyword(row, keyword)) {
            result.append(row);
        }
    }
    return result;
}

QJsonArray DemoRepository::activeConsultations(const QString& keyword,
                                               const QString& departmentFilter,
                                               const QString& doctorFilter,
                                               const QString& clinicTypeFilter)
{
    QMutexLocker locker(&m_mutex);
    QJsonArray result;
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        const QString status = registration.value("状态").toString();
        if (status != "CALLED" && status != "IN_CONSULTATION" && status != "CHECK_DONE") {
            continue;
        }

        QString title;
        double fee = 0.0;
        for (const auto& doctorItem : m_doctors) {
            const auto doctor = doctorItem.toObject();
            if (doctor.value("医生姓名").toString() == registration.value("医生").toString()) {
                title = doctor.value("职称").toString();
                fee = doctor.value("挂号费").toVariant().toDouble();
                break;
            }
        }

        QString complaint;
        QString presentIllness;
        QString pastHistory;
        QString physicalSign;
        QString icdCode;
        QString diagnosis;
        QString advice;
        QString externalReportHospital;
        QString externalReportType;
        QString externalReportDate;
        QString externalReportSummary;
        QString externalReportConclusion;
        QString externalReportAttachment;
        QString consultationTime;
        for (const auto& recordItem : m_consultations) {
            const auto record = recordItem.toObject();
            if (record.value("挂号单号").toString() == registration.value("挂号单号").toString()) {
                complaint = record.value("主诉").toString();
                presentIllness = record.value("现病史").toString();
                pastHistory = record.value("既往史").toString();
                physicalSign = record.value("体格检查").toString();
                icdCode = record.value("ICD编码").toString();
                diagnosis = record.value("诊断").toString();
                advice = record.value("医嘱").toString();
                externalReportHospital = record.value("外院报告医院").toString();
                externalReportType = record.value("外院报告类型").toString();
                externalReportDate = record.value("外院报告日期").toString();
                externalReportSummary = record.value("外院报告摘要").toString();
                externalReportConclusion = record.value("外院报告结论").toString();
                externalReportAttachment = record.value("外院报告附件").toString();
                consultationTime = record.value("接诊时间").toString();
                break;
            }
        }

        QJsonObject row;
        row["科室"] = registration.value("科室").toString();
        row["医生"] = registration.value("医生").toString();
        row["职称"] = title;
        row["号别"] = title.contains("主任") || fee >= 30.0 ? "专家号" : "普通号";
        row["患者"] = registration.value("患者").toString();
        row["身份证号"] = patientFieldByName(m_patients, row.value("患者").toString(), "身份证号");
        row["挂号单号"] = registration.value("挂号单号").toString();
        row["就诊日期"] = registration.value("就诊日期").toString();
        row["时段"] = registration.value("时段").toString();
        row["状态"] = registrationStatusText(status);
        row["主诉"] = complaint;
        row["现病史"] = presentIllness;
        row["既往史"] = pastHistory;
        row["体格检查"] = physicalSign;
        row["ICD编码"] = icdCode;
        row["诊断"] = diagnosis;
        row["医嘱"] = advice;
        row["外院报告医院"] = externalReportHospital;
        row["外院报告类型"] = externalReportType;
        row["外院报告日期"] = externalReportDate;
        row["外院报告摘要"] = externalReportSummary;
        row["外院报告结论"] = externalReportConclusion;
        row["外院报告附件"] = externalReportAttachment;
        row["接诊时间"] = consultationTime;
        if (!departmentFilter.isEmpty() && row.value("科室").toString() != departmentFilter) {
            continue;
        }
        if (!doctorFilter.isEmpty() && row.value("医生").toString() != doctorFilter) {
            continue;
        }
        if (!clinicTypeFilter.isEmpty() && row.value("号别").toString() != clinicTypeFilter) {
            continue;
        }
        if (containsKeyword(row, keyword)) {
            result.append(row);
        }
    }
    return result;
}

QJsonObject DemoRepository::createExamination(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString itemName = payload.value("检查项目").toString().trimmed();
    if (registrationNo.isEmpty() || itemName.isEmpty()) {
        return makeResult(false, "挂号单号和检查项目不能为空。");
    }

    QString patient;
    QString doctor;
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            patient = registration.value("患者").toString();
            doctor = registration.value("医生").toString();
            if (registration.value("状态").toString() != "FINISHED") {
                registration["状态"] = "CHECKING";
                m_registrations.replace(i, registration);
            }
            break;
        }
    }
    if (patient.isEmpty()) {
        return makeResult(false, "未找到该挂号单，请先完成挂号。");
    }

    const QString examNo = "EX" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    m_examinations.prepend(row({
        {"检查单号", examNo},
        {"挂号单号", registrationNo},
        {"患者", patient},
        {"医生", doctor},
        {"检查项目", itemName},
        {"申请说明", payload.value("申请说明").toString()},
        {"检查结果", ""},
        {"状态", "待检查"},
        {"申请时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")},
        {"完成时间", ""}
    }));

    auto result = makeResult(true, "检查申请已开立，患者已转入检查中。");
    result["检查单号"] = examNo;
    return result;
}

QJsonObject DemoRepository::completeExamination(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString examNo = payload.value("检查单号").toString();
    const QString resultText = payload.value("检查结果").toString().trimmed();
    if (examNo.isEmpty() || resultText.isEmpty()) {
        return makeResult(false, "检查单号和检查结果不能为空。");
    }

    for (int i = 0; i < m_examinations.size(); ++i) {
        auto exam = m_examinations.at(i).toObject();
        if (exam.value("检查单号").toString() == examNo) {
            const QString registrationNo = exam.value("挂号单号").toString();
            exam["检查结果"] = resultText;
            exam["状态"] = "已完成";
            exam["完成时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_examinations.replace(i, exam);
            for (int j = 0; j < m_registrations.size(); ++j) {
                auto registration = m_registrations.at(j).toObject();
                if (registration.value("挂号单号").toString() == registrationNo
                    && registration.value("状态").toString() == "CHECKING") {
                    registration["状态"] = "CHECK_DONE";
                    m_registrations.replace(j, registration);
                    break;
                }
            }
            return makeResult(true, "检查结果已回传，患者已进入检查完成待复诊。");
        }
    }
    return makeResult(false, "未找到该检查单。");
}

QJsonObject DemoRepository::updatePatientRecord(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    if (registrationNo.isEmpty()) {
        return makeResult(false, "缺少挂号单号，无法修改病历。");
    }

    QString patient;
    QString doctor;
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            patient = registration.value("患者").toString();
            doctor = registration.value("医生").toString();
            break;
        }
    }
    if (patient.isEmpty()) {
        return makeResult(false, "未找到对应就诊记录。");
    }

    const QJsonObject record = row({
        {"挂号单号", registrationNo},
        {"患者", patient},
        {"医生", doctor},
        {"主诉", payload.value("主诉").toString()},
        {"现病史", payload.value("现病史").toString()},
        {"既往史", payload.value("既往史").toString()},
        {"体格检查", payload.value("体格检查").toString()},
        {"ICD编码", payload.value("ICD编码").toString()},
        {"诊断", payload.value("诊断").toString()},
        {"医嘱", payload.value("医嘱").toString()},
        {"外院报告医院", payload.value("外院报告医院").toString()},
        {"外院报告类型", payload.value("外院报告类型").toString()},
        {"外院报告日期", payload.value("外院报告日期").toString()},
        {"外院报告摘要", payload.value("外院报告摘要").toString()},
        {"外院报告结论", payload.value("外院报告结论").toString()},
        {"外院报告附件", payload.value("外院报告附件").toString()},
        {"接诊时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    });

    for (int i = 0; i < m_consultations.size(); ++i) {
        if (m_consultations.at(i).toObject().value("挂号单号").toString() == registrationNo) {
            m_consultations.replace(i, record);
            return makeResult(true, "病历内容已修改。");
        }
    }
    m_consultations.prepend(record);
    return makeResult(true, "病历内容已补录。");
}

QJsonObject DemoRepository::deletePatientRecord(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    if (registrationNo.isEmpty()) {
        return makeResult(false, "缺少挂号单号，无法作废病历。");
    }

    for (int i = 0; i < m_consultations.size(); ++i) {
        auto record = m_consultations.at(i).toObject();
        if (record.value("挂号单号").toString() == registrationNo) {
            record["主诉"] = "病历已作废";
            record["现病史"] = "病历已作废";
            record["既往史"] = "病历已作废";
            record["体格检查"] = "病历已作废";
            record["ICD编码"] = "";
            record["诊断"] = "病历已作废";
            record["医嘱"] = "病历已作废";
            record["外院报告医院"] = "";
            record["外院报告类型"] = "";
            record["外院报告日期"] = "";
            record["外院报告摘要"] = "";
            record["外院报告结论"] = "";
            record["外院报告附件"] = "";
            record["接诊时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_consultations.replace(i, record);
            return makeResult(true, "病历内容已作废，就诊流水已保留。");
        }
    }
    return makeResult(false, "该就诊记录还没有病历内容，无需作废。");
}

QJsonObject DemoRepository::saveDepartment(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    QString code = payload.value("科室编码").toString().trimmed();
    const QString name = payload.value("科室名称").toString().trimmed();
    if (name.isEmpty()) {
        return makeResult(false, "科室名称不能为空。");
    }
    if (code.isEmpty()) {
        code = QString("DEP%1").arg(m_departments.size() + 1, 3, 10, QChar('0'));
    }

    for (int i = 0; i < m_departments.size(); ++i) {
        auto department = m_departments.at(i).toObject();
        if (department.value("科室编码").toString() == code
            || department.value("科室名称").toString() == name) {
            department["科室编码"] = code;
            department["科室名称"] = name;
            department["位置"] = payload.value("位置").toString();
            department["状态"] = 1;
            m_departments.replace(i, department);
            return makeResult(true, "科室已更新。");
        }
    }

    m_departments.prepend(row({
        {"科室编码", code},
        {"科室名称", name},
        {"位置", payload.value("位置").toString()},
        {"状态", 1}
    }));
    return makeResult(true, "科室已保存。");
}

QJsonObject DemoRepository::updateDepartment(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString oldCode = payload.value("原科室编码").toString(payload.value("科室编码").toString());
    for (int i = 0; i < m_departments.size(); ++i) {
        auto department = m_departments.at(i).toObject();
        if (department.value("科室编码").toString() == oldCode) {
            department["科室编码"] = payload.value("科室编码").toString();
            department["科室名称"] = payload.value("科室名称").toString();
            department["位置"] = payload.value("位置").toString();
            department["状态"] = payload.contains("状态") ? payload.value("状态").toVariant().toInt() : 1;
            m_departments.replace(i, department);
            return makeResult(true, "科室信息已修改。");
        }
    }
    return makeResult(false, "未找到要修改的科室。");
}

QJsonObject DemoRepository::disableDepartment(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString code = payload.value("科室编码").toString();
    for (int i = 0; i < m_departments.size(); ++i) {
        auto department = m_departments.at(i).toObject();
        if (department.value("科室编码").toString() == code) {
            department["状态"] = 0;
            m_departments.replace(i, department);
            return makeResult(true, "科室已停用。");
        }
    }
    return makeResult(false, "未找到要停用的科室。");
}

QJsonObject DemoRepository::saveDoctor(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString name = payload.value("医生姓名").toString().trimmed();
    const QString department = payload.value("所属科室").toString().trimmed();
    if (name.isEmpty() || department.isEmpty()) {
        return makeResult(false, "医生姓名和所属科室不能为空。");
    }

    bool departmentExists = false;
    for (const auto& item : m_departments) {
        const auto object = item.toObject();
        const int status = object.contains("状态") ? object.value("状态").toVariant().toInt() : 1;
        if (object.value("科室名称").toString() == department && status == 1) {
            departmentExists = true;
            break;
        }
    }
    if (!departmentExists) {
        const QString code = QString("DEP%1").arg(m_departments.size() + 1, 3, 10, QChar('0'));
        m_departments.prepend(row({{"科室编码", code}, {"科室名称", department}, {"位置", ""}, {"状态", 1}}));
    }

    for (int i = 0; i < m_doctors.size(); ++i) {
        auto doctor = m_doctors.at(i).toObject();
        if (doctor.value("医生姓名").toString() == name) {
            doctor["所属科室"] = department;
            doctor["职称"] = payload.value("职称").toString();
            doctor["擅长方向"] = payload.value("擅长方向").toString();
            doctor["电话"] = payload.value("电话").toString();
            doctor["挂号费"] = payload.value("挂号费").toVariant().toDouble();
            doctor["状态"] = 1;
            m_doctors.replace(i, doctor);
            return makeResult(true, "医生已更新。");
        }
    }

    m_doctors.prepend(row({
        {"医生姓名", name},
        {"所属科室", department},
        {"职称", payload.value("职称").toString()},
        {"擅长方向", payload.value("擅长方向").toString()},
        {"挂号费", payload.value("挂号费").toVariant().toDouble()},
        {"电话", payload.value("电话").toString()},
        {"状态", 1}
    }));
    return makeResult(true, "医生已新增。请到医生排班为该医生设置号源。");
}

QJsonObject DemoRepository::updateDoctor(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString oldName = payload.value("原医生姓名").toString(payload.value("医生姓名").toString());
    for (int i = 0; i < m_doctors.size(); ++i) {
        auto doctor = m_doctors.at(i).toObject();
        if (doctor.value("医生姓名").toString() == oldName) {
            doctor["医生姓名"] = payload.value("医生姓名").toString();
            doctor["所属科室"] = payload.value("所属科室").toString();
            doctor["职称"] = payload.value("职称").toString();
            doctor["擅长方向"] = payload.value("擅长方向").toString();
            doctor["挂号费"] = payload.value("挂号费").toVariant().toDouble();
            doctor["电话"] = payload.value("电话").toString();
            doctor["状态"] = payload.contains("状态") ? payload.value("状态").toVariant().toInt() : 1;
            m_doctors.replace(i, doctor);
            return makeResult(true, "医生信息已修改。");
        }
    }
    return makeResult(false, "未找到要修改的医生。");
}

QJsonObject DemoRepository::disableDoctor(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString name = payload.value("医生姓名").toString();
    bool found = false;
    for (int i = 0; i < m_doctors.size(); ++i) {
        auto doctor = m_doctors.at(i).toObject();
        if (doctor.value("医生姓名").toString() == name) {
            doctor["状态"] = 0;
            m_doctors.replace(i, doctor);
            found = true;
        }
    }
    for (int i = 0; i < m_schedules.size(); ++i) {
        auto schedule = m_schedules.at(i).toObject();
        if (schedule.value("医生").toString() == name) {
            schedule["状态"] = 0;
            schedule["剩余号源"] = 0;
            m_schedules.replace(i, schedule);
        }
    }
    return makeResult(found, found ? "医生已停用。" : "未找到要停用的医生。");
}

QJsonObject DemoRepository::addRegistration(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString doctor = payload.value("doctor").toString();
    const QString department = payload.value("department").toString().trimmed();
    const QString date = payload.value("date").toString();
    const QString period = "全天";
    const int scheduleIndex = findScheduleIndex(doctor, date, period, department);

    if (scheduleIndex < 0) {
        return makeResult(false, "未找到该医生当天排班，请先在医生排班中维护号源。");
    }

    auto schedule = m_schedules.at(scheduleIndex).toObject();
    const int remain = schedule.value("剩余号源").toInt();
    if (remain <= 0) {
        return makeResult(false, "该医生当天号源已满。");
    }

    schedule["剩余号源"] = remain - 1;
    m_schedules.replace(scheduleIndex, schedule);

    const QString patientName = payload.value("patientName").toString().trimmed();
    const QString phone = payload.value("phone").toString().trimmed();
    const QString idCard = payload.value("idCard").toString().trimmed();
    bool patientExists = false;
    int previousVisitCount = 0;
    for (int i = 0; i < m_patients.size(); ++i) {
        auto patient = m_patients.at(i).toObject();
        if ((!phone.isEmpty() && patient.value("电话").toString() == phone)
            || (!idCard.isEmpty() && patient.value("身份证号").toString() == idCard)
            || patient.value("姓名").toString() == patientName) {
            previousVisitCount = patient.value("就诊次数").toVariant().toInt();
            patient["姓名"] = patientName.isEmpty() ? patient.value("姓名") : patientName;
            patient["电话"] = phone.isEmpty() ? patient.value("电话") : phone;
            patient["身份证号"] = idCard.isEmpty() ? patient.value("身份证号") : idCard;
            patient["身份登记"] = (!phone.isEmpty() || !idCard.isEmpty()) ? "已登记" : patient.value("身份登记");
            patient["患者状态"] = "已确认患者";
            patient["就诊次数"] = patient.value("就诊次数").toVariant().toInt() + 1;
            patient["最近就诊"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_patients.replace(i, patient);
            patientExists = true;
            break;
        }
    }
    if (!patientExists) {
        m_patients.prepend(row({
            {"患者编号", QString("P%1%2").arg(QDateTime::currentDateTime().toString("yyyyMMdd")).arg(m_patients.size() + 1, 4, 10, QChar('0'))},
            {"姓名", patientName},
            {"性别", payload.value("gender").toString("未知")},
            {"电话", phone},
            {"身份证号", idCard},
            {"身份登记", (!phone.isEmpty() || !idCard.isEmpty()) ? "已登记" : "待补全"},
            {"患者状态", "已确认患者"},
            {"就诊次数", 1},
            {"最近就诊", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")},
            {"地址", payload.value("address").toString()},
            {"建档时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
        }));
    }

    const QString no = QString("R%1%2")
        .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss"))
        .arg(m_registrations.size() + 1, 3, 10, QChar('0'));
    double fee = payload.value("fee").toDouble(0.00);
    if (fee <= 0.0) {
        for (const auto& item : m_doctors) {
            const auto doctorRow = item.toObject();
            if (doctorRow.value("医生姓名").toString() == doctor) {
                fee = doctorRow.value("挂号费").toVariant().toDouble();
                break;
            }
        }
    }
    if (fee <= 0.0) {
        fee = 20.00;
    }

    m_registrations.prepend(row({
        {"挂号单号", no},
        {"患者", patientName},
        {"科室", payload.value("department").toString()},
        {"医生", doctor},
        {"就诊日期", date},
        {"时段", payload.value("timeSlot").toString()},
        {"状态", "WAITING"},
        {"挂号费", fee},
        {"挂号时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    }));

    m_bills.prepend(row({
        {"账单号", "B" + no.mid(1)},
        {"患者", patientName},
        {"挂号费", fee},
        {"药品费", 0.00},
        {"其他费用", 0.00},
        {"合计", fee},
        {"状态", "UNPAID"},
        {"创建时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    }));

    auto result = makeResult(true, patientExists
        ? QString("预约挂号成功，已同步到医院端挂号管理。系统识别到该患者已有档案，疑似复诊或重复患者，历史就诊 %1 次。").arg(previousVisitCount)
        : QString("预约挂号成功，已同步到医院端挂号管理。"));
    result["registrationNo"] = no;
    result["duplicatePatient"] = patientExists;
    result["previousVisitCount"] = previousVisitCount;
    return result;
}

QJsonObject DemoRepository::updateRegistration(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            registration["患者"] = payload.value("患者").toString(registration.value("患者").toString());
            registration["科室"] = payload.value("科室").toString(registration.value("科室").toString());
            registration["医生"] = payload.value("医生").toString(registration.value("医生").toString());
            registration["就诊日期"] = payload.value("就诊日期").toString(registration.value("就诊日期").toString());
            registration["时段"] = payload.value("时段").toString(registration.value("时段").toString());
            registration["状态"] = payload.value("状态").toString(registration.value("状态").toString());
            registration["挂号费"] = payload.value("挂号费").toVariant().toDouble();
            m_registrations.replace(i, registration);
            return makeResult(true, "挂号记录已修改。");
        }
    }
    return makeResult(false, "未找到要修改的挂号记录。");
}

QJsonObject DemoRepository::cancelRegistration(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() != registrationNo) {
            continue;
        }

        const QString status = registration.value("状态").toString();
        if (status == "FINISHED") {
            return makeResult(false, "该挂号已接诊，不能取消。");
        }
        if (status == "CANCELLED") {
            return makeResult(true, "该挂号已经是取消状态。");
        }

        registration["状态"] = "CANCELLED";
        m_registrations.replace(i, registration);

        const QString doctor = registration.value("医生").toString();
        const QString date = registration.value("就诊日期").toString();
        const QString period = registration.value("时段").toString().section(' ', 0, 0);
        const int scheduleIndex = findScheduleIndex(doctor, date, period);
        if (scheduleIndex >= 0) {
            auto schedule = m_schedules.at(scheduleIndex).toObject();
            const int total = schedule.value("总号源").toInt();
            const int remain = schedule.value("剩余号源").toInt();
            schedule["剩余号源"] = qMin(total, remain + 1);
            m_schedules.replace(scheduleIndex, schedule);
        }
        return makeResult(true, "挂号记录已取消，号源已退回。");
    }
    return makeResult(false, "未找到要取消的挂号记录。");
}

QJsonObject DemoRepository::callRegistration(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            const QString status = registration.value("状态").toString();
            if (status != "WAITING" && status != "CHECK_DONE") {
                return makeResult(false, "只有待叫号或检查完成待复诊的患者可以叫号。");
            }
            registration["状态"] = "CALLED";
            registration["叫号时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_registrations.replace(i, registration);
            return makeResult(true, "叫号成功，患者状态已更新。");
        }
    }
    return makeResult(false, "未找到要叫号的挂号记录。");
}

QJsonObject DemoRepository::startConsultation(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            const QString status = registration.value("状态").toString();
            if (status != "CALLED" && status != "CHECK_DONE") {
                return makeResult(false, "只有已叫号或检查完成待复诊的患者可以开始接诊。");
            }
            registration["状态"] = "IN_CONSULTATION";
            m_registrations.replace(i, registration);
            return makeResult(true, "已进入接诊中。");
        }
    }
    return makeResult(false, "未找到要接诊的挂号记录。");
}

QJsonObject DemoRepository::saveSchedule(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString doctor = payload.value("doctor").toString();
    const QString date = payload.value("date").toString();
    const QString period = "全天";
    const int quota = payload.value("quota").toInt(30);
    const QDate workDate = QDate::fromString(date, "yyyy-MM-dd");
    if (doctor.trimmed().isEmpty() || !workDate.isValid()) {
        return makeResult(false, "医生和出诊日期不能为空。");
    }
    if (workDate < QDate::currentDate()) {
        return makeResult(false, "不能为过去日期排班。");
    }
    if (quota <= 0) {
        return makeResult(false, "号源数量必须大于 0。");
    }
    const int index = findScheduleIndex(doctor, date, period);

    if (index >= 0) {
        auto schedule = m_schedules.at(index).toObject();
        const int used = schedule.value("总号源").toInt() - schedule.value("剩余号源").toInt();
        schedule["科室"] = payload.value("department").toString(schedule.value("科室").toString());
        schedule["职称"] = payload.value("title").toString(schedule.value("职称").toString());
        schedule["总号源"] = quota;
        schedule["剩余号源"] = qMax(0, quota - used);
        schedule["状态"] = 1;
        m_schedules.replace(index, schedule);
    } else {
        m_schedules.prepend(row({
            {"科室", payload.value("department").toString("待维护")},
            {"医生", doctor},
            {"职称", payload.value("title").toString("医师")},
            {"出诊日期", date},
            {"时段", period},
            {"总号源", quota},
            {"剩余号源", quota},
            {"状态", 1}
        }));
    }

    return makeResult(true, "排班号源已保存。");
}

QJsonObject DemoRepository::disableSchedule(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString doctor = payload.value("医生").toString(payload.value("doctor").toString());
    const QString date = payload.value("出诊日期").toString(payload.value("date").toString());
    int disabled = 0;
    for (int i = 0; i < m_schedules.size(); ++i) {
        auto schedule = m_schedules.at(i).toObject();
        if (schedule.value("医生").toString() != doctor
            || schedule.value("出诊日期").toString() != date
            || schedule.value("状态").toVariant().toInt() == 0) {
            continue;
        }
        schedule["状态"] = 0;
        schedule["剩余号源"] = 0;
        m_schedules.replace(i, schedule);
        ++disabled;
    }
    if (disabled == 0) {
        return makeResult(false, "未找到要停诊的排班。");
    }

    return makeResult(true, "排班已停诊。");
}

QJsonObject DemoRepository::resetSchedules()
{
    QMutexLocker locker(&m_mutex);
    int resetCount = 0;
    for (int i = 0; i < m_schedules.size(); ++i) {
        auto schedule = m_schedules.at(i).toObject();
        if (schedule.value("状态").toVariant().toInt() == 0) {
            continue;
        }
        schedule["状态"] = 0;
        schedule["剩余号源"] = 0;
        m_schedules.replace(i, schedule);
        ++resetCount;
    }

    auto result = makeResult(true, QString("已清空 %1 条排班数据，可以重新排班。").arg(resetCount));
    result["resetCount"] = resetCount;
    return result;
}

QJsonObject DemoRepository::saveConsultation(const QJsonObject& payload, bool backToWaiting)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString();
    if (registrationNo.isEmpty()) {
        return makeResult(false, "缺少挂号单号，无法保存接诊记录。");
    }

    QString patient;
    QString doctor;
    for (int i = 0; i < m_registrations.size(); ++i) {
        auto registration = m_registrations.at(i).toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            const QString status = registration.value("状态").toString();
            if (status != "CALLED" && status != "IN_CONSULTATION" && status != "CHECK_DONE") {
                return makeResult(false, "当前状态不能保存接诊，请先叫号或等待检查结果回传。");
            }
            patient = registration.value("患者").toString();
            doctor = registration.value("医生").toString();
            registration["状态"] = backToWaiting ? "CHECKING" : "FINISHED";
            m_registrations.replace(i, registration);
            break;
        }
    }

    if (patient.isEmpty() || doctor.isEmpty()) {
        return makeResult(false, "未找到对应挂号记录，请刷新候诊队列后重试。");
    }

    const QJsonObject record = row({
        {"挂号单号", registrationNo},
        {"患者", patient},
        {"医生", doctor},
        {"主诉", payload.value("主诉").toString()},
        {"现病史", payload.value("现病史").toString()},
        {"既往史", payload.value("既往史").toString()},
        {"体格检查", payload.value("体格检查").toString()},
        {"ICD编码", payload.value("ICD编码").toString()},
        {"诊断", backToWaiting && payload.value("诊断").toString().trimmed().isEmpty()
                     ? QStringLiteral("待检查结果回报")
                     : payload.value("诊断").toString()},
        {"医嘱", payload.value("医嘱").toString()},
        {"外院报告医院", payload.value("外院报告医院").toString()},
        {"外院报告类型", payload.value("外院报告类型").toString()},
        {"外院报告日期", payload.value("外院报告日期").toString()},
        {"外院报告摘要", payload.value("外院报告摘要").toString()},
        {"外院报告结论", payload.value("外院报告结论").toString()},
        {"外院报告附件", payload.value("外院报告附件").toString()},
        {"接诊时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    });

    for (int i = 0; i < m_consultations.size(); ++i) {
        if (m_consultations.at(i).toObject().value("挂号单号").toString() == registrationNo) {
            m_consultations.replace(i, record);
            return makeResult(true, backToWaiting ? "已保存当前接诊记录，患者已转入检查中，检查完成后可复诊。" : "接诊记录已更新。");
        }
    }

    m_consultations.prepend(record);
    return makeResult(true, backToWaiting ? "已保存当前接诊记录，患者已转入检查中，检查完成后可复诊。" : "接诊记录已保存，挂号状态已改为 FINISHED。");
}

QJsonObject DemoRepository::createPrescription(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString registrationNo = payload.value("挂号单号").toString().trimmed();
    const QString drugName = payload.value("药品名称").toString().trimmed();
    const int quantity = payload.value("数量").toVariant().toInt();
    if (registrationNo.isEmpty() || drugName.isEmpty() || quantity <= 0) {
        return makeResult(false, "挂号单号、药品名称和数量不能为空。");
    }

    QString patient;
    QString doctor;
    bool finished = false;
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        if (registration.value("挂号单号").toString() == registrationNo) {
            patient = registration.value("患者").toString();
            doctor = registration.value("医生").toString();
            finished = registration.value("状态").toString() == "FINISHED";
            break;
        }
    }
    if (patient.isEmpty()) {
        return makeResult(false, "未找到该挂号单，请先完成挂号。");
    }
    if (!finished) {
        return makeResult(false, "该患者还未完成接诊，请先在医生接诊中保存病历后再开处方。");
    }

    double unitPrice = 0.0;
    for (int i = 0; i < m_inventory.size(); ++i) {
        auto drug = m_inventory.at(i).toObject();
        if (drug.value("药品名称").toString() == drugName
            || drug.value("药品编码").toString() == drugName
            || drug.value("条形码").toString() == drugName) {
            const int stock = drug.value("库存").toVariant().toInt();
            if (stock < quantity) {
                return makeResult(false, QString("库存不足，当前库存 %1。").arg(stock));
            }
            unitPrice = drug.value("售价").toVariant().toDouble();
            break;
        }
    }
    if (unitPrice <= 0.0) {
        return makeResult(false, "药品库存中未找到该药品，请先在药品库存中入库。");
    }

    const double amount = unitPrice * quantity;
    const QString prescriptionNo = "RX" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    m_prescriptions.prepend(row({
        {"处方号", prescriptionNo},
        {"挂号单号", registrationNo},
        {"患者", patient},
        {"医生", doctor},
        {"状态", "待审核"},
        {"药品明细", QString("%1 x%2").arg(drugName).arg(quantity)},
        {"药品名称", drugName},
        {"数量", quantity},
        {"处方金额", amount},
        {"开方时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    }));

    for (int i = 0; i < m_bills.size(); ++i) {
        auto bill = m_bills.at(i).toObject();
        if (bill.value("患者").toString() == patient && bill.value("状态").toString() != "PAID") {
            const double drugFee = bill.value("药品费").toVariant().toDouble() + amount;
            const double total = bill.value("合计").toVariant().toDouble() + amount;
            bill["药品费"] = drugFee;
            bill["合计"] = total;
            m_bills.replace(i, bill);
            break;
        }
    }

    auto result = makeResult(true, "处方已开立，等待药师审核。");
    result["处方号"] = prescriptionNo;
    result["处方金额"] = amount;
    return result;
}

QJsonObject DemoRepository::reviewPrescription(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString prescriptionNo = payload.value("处方号").toString();
    for (int i = 0; i < m_prescriptions.size(); ++i) {
        auto prescription = m_prescriptions.at(i).toObject();
        if (prescription.value("处方号").toString() == prescriptionNo) {
            if (prescription.value("状态").toString() == "已发药") {
                return makeResult(false, "已发药处方不能重复审核。");
            }
            if (prescription.value("状态").toString() == "已驳回") {
                return makeResult(false, "已驳回处方不能审核，请医生重新开立处方。");
            }
            prescription["状态"] = "待发药";
            prescription["审核人"] = payload.value("__operatorName").toString("药师");
            prescription["审核时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_prescriptions.replace(i, prescription);
            m_operationLogs.prepend(row({
                {"操作人", payload.value("__operatorName").toString("药师")},
                {"模块", "处方管理"},
                {"动作", "审核处方"},
                {"内容", "审核通过 " + prescriptionNo},
                {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
            }));
            return makeResult(true, "处方审核通过，等待发药。");
        }
    }
    return makeResult(false, "未找到该处方。");
}

QJsonObject DemoRepository::rejectPrescription(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString prescriptionNo = payload.value("处方号").toString();
    const QString rejectReason = payload.value("驳回原因").toString().trimmed();
    if (rejectReason.isEmpty()) {
        return makeResult(false, "驳回处方必须填写驳回原因。");
    }

    for (int i = 0; i < m_prescriptions.size(); ++i) {
        auto prescription = m_prescriptions.at(i).toObject();
        if (prescription.value("处方号").toString() != prescriptionNo) {
            continue;
        }
        const QString status = prescription.value("状态").toString();
        if (status != "待审核" && status != "CREATED") {
            return makeResult(false, "只有待审核处方可以驳回。");
        }

        prescription["状态"] = "已驳回";
        prescription["驳回原因"] = rejectReason;
        prescription["审核人"] = payload.value("__operatorName").toString("药师");
        prescription["审核时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        m_prescriptions.replace(i, prescription);
        m_operationLogs.prepend(row({
            {"操作人", payload.value("__operatorName").toString("药师")},
            {"模块", "处方管理"},
            {"动作", "驳回处方"},
            {"内容", QString("驳回 %1：%2").arg(prescriptionNo, rejectReason)},
            {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
        }));
        return makeResult(true, "处方已驳回，医生需重新开立处方。");
    }
    return makeResult(false, "未找到该处方。");
}

QJsonObject DemoRepository::dispensePrescription(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString prescriptionNo = payload.value("处方号").toString();
    for (int i = 0; i < m_prescriptions.size(); ++i) {
        auto prescription = m_prescriptions.at(i).toObject();
        if (prescription.value("处方号").toString() != prescriptionNo) {
            continue;
        }
        if (prescription.value("状态").toString() == "已发药") {
            return makeResult(true, "该处方已发药。");
        }
        if (prescription.value("状态").toString() == "已驳回") {
            return makeResult(false, "该处方已驳回，不能发药。请医生重新开立处方。");
        }
        if (prescription.value("状态").toString() == "已退药") {
            return makeResult(false, "该处方已退药，不能再次发药。");
        }
        if (prescription.value("状态").toString() != "待发药") {
            return makeResult(false, "处方需先审核通过后才能发药。");
        }

        const QString registrationNo = prescription.value("挂号单号").toString();
        bool paid = false;
        bool billFound = false;
        for (const auto& billItem : m_bills) {
            const auto bill = billItem.toObject();
            if (bill.value("挂号单号").toString() == registrationNo) {
                billFound = true;
                paid = bill.value("状态").toString() == "PAID";
                break;
            }
        }
        if (!billFound) {
            return makeResult(false, "未找到该处方对应账单，不能发药。");
        }
        if (!paid) {
            return makeResult(false, "该处方对应账单未缴费，不能发药。请先完成收费结算。");
        }

        const QString drugName = prescription.value("药品名称").toString();
        const int quantity = prescription.value("数量").toVariant().toInt();
        for (int j = 0; j < m_inventory.size(); ++j) {
            auto drug = m_inventory.at(j).toObject();
            if (drug.value("药品名称").toString() == drugName) {
                const int stock = drug.value("库存").toVariant().toInt();
                if (stock < quantity) {
                    return makeResult(false, QString("库存不足，当前库存 %1。").arg(stock));
                }
                drug["库存"] = stock - quantity;
                m_inventory.replace(j, drug);
                prescription["状态"] = "已发药";
                prescription["发药人"] = payload.value("__operatorName").toString("药房");
                prescription["发药时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                m_prescriptions.replace(i, prescription);
                m_operationLogs.prepend(row({
                    {"操作人", payload.value("__operatorName").toString("药房")},
                    {"模块", "处方管理"},
                    {"动作", "确认发药"},
                    {"内容", "发药完成 " + prescriptionNo},
                    {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
                }));
                return makeResult(true, "发药成功，库存已扣减。");
            }
        }
        return makeResult(false, "药品库存中未找到该药品。");
    }
    return makeResult(false, "未找到该处方。");
}

QJsonObject DemoRepository::returnPrescription(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString prescriptionNo = payload.value("处方号").toString();
    const QString returnReason = payload.value("退药原因").toString().trimmed();
    if (returnReason.isEmpty()) {
        return makeResult(false, "退药必须填写退药原因。");
    }

    for (int i = 0; i < m_prescriptions.size(); ++i) {
        auto prescription = m_prescriptions.at(i).toObject();
        if (prescription.value("处方号").toString() != prescriptionNo) {
            continue;
        }
        if (prescription.value("状态").toString() == "已退药") {
            return makeResult(false, "该处方已退药，不能重复退药。");
        }
        if (prescription.value("状态").toString() != "已发药") {
            return makeResult(false, "只有已发药处方才能退药。");
        }

        const QString drugName = prescription.value("药品名称").toString();
        const int quantity = prescription.value("数量").toVariant().toInt();
        for (int j = 0; j < m_inventory.size(); ++j) {
            auto drug = m_inventory.at(j).toObject();
            if (drug.value("药品名称").toString() == drugName) {
                const int stock = drug.value("库存").toVariant().toInt();
                drug["库存"] = stock + quantity;
                m_inventory.replace(j, drug);
                prescription["状态"] = "已退药";
                prescription["退药原因"] = returnReason;
                prescription["退药人"] = payload.value("__operatorName").toString("药房");
                prescription["退药时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                m_prescriptions.replace(i, prescription);
                m_operationLogs.prepend(row({
                    {"操作人", payload.value("__operatorName").toString("药房")},
                    {"模块", "处方管理"},
                    {"动作", "退药入库"},
                    {"内容", QString("退药 %1：%2").arg(prescriptionNo, returnReason)},
                    {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
                }));
                return makeResult(true, "退药完成，库存已回加。");
            }
        }
        return makeResult(false, "药品库存中未找到该药品。");
    }
    return makeResult(false, "未找到该处方。");
}

QJsonObject DemoRepository::updatePatient(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString patientNo = payload.value("患者编号").toString();
    for (int i = 0; i < m_patients.size(); ++i) {
        auto patient = m_patients.at(i).toObject();
        if (patient.value("患者编号").toString() == patientNo) {
            patient["姓名"] = payload.value("姓名").toString(patient.value("姓名").toString());
            patient["性别"] = payload.value("性别").toString(patient.value("性别").toString());
            patient["电话"] = payload.value("电话").toString(patient.value("电话").toString());
            patient["身份证号"] = payload.value("身份证号").toString(patient.value("身份证号").toString());
            patient["地址"] = payload.value("地址").toString(patient.value("地址").toString());
            patient["身份登记"] = (!patient.value("电话").toString().isEmpty() || !patient.value("身份证号").toString().isEmpty()) ? "已登记" : "待补全";
            m_patients.replace(i, patient);
            return makeResult(true, "患者信息已修改。");
        }
    }
    return makeResult(false, "未找到要修改的患者。");
}

QJsonObject DemoRepository::deletePatient(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString patientNo = payload.value("患者编号").toString();
    const QString patientName = payload.value("姓名").toString();
    for (const auto& item : m_registrations) {
        const auto registration = item.toObject();
        if (registration.value("患者").toString() == patientName
            && registration.value("状态").toString() != "CANCELLED") {
            return makeResult(false, "该患者已有挂号/接诊记录，为保护病历数据不能直接删除。");
        }
    }
    for (int i = 0; i < m_patients.size(); ++i) {
        if (m_patients.at(i).toObject().value("患者编号").toString() == patientNo) {
            m_patients.removeAt(i);
            return makeResult(true, "患者信息已删除。");
        }
    }
    return makeResult(false, "未找到要删除的患者。");
}

QJsonObject DemoRepository::addInventory(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString barcode = payload.value("barcode").toString();
    const QString name = payload.value("drugName").toString();
    const int quantity = payload.value("quantity").toInt();

    for (int i = 0; i < m_inventory.size(); ++i) {
        auto drug = m_inventory.at(i).toObject();
        if ((!barcode.isEmpty() && drug.value("条形码").toString() == barcode)
            || drug.value("药品名称").toString() == name) {
            drug["库存"] = drug.value("库存").toInt() + quantity;
            if (!barcode.isEmpty()) {
                drug["条形码"] = barcode;
            }
            drug["有效期"] = payload.value("expiryDate").toString(drug.value("有效期").toString());
            drug["预警原因"] = inventoryWarningReason(drug);
            m_inventory.replace(i, drug);
            return makeResult(true, "已有药品库存已增加。");
        }
    }

    const QString code = QString("D%1").arg(m_inventory.size() + 1, 3, 10, QChar('0'));
    m_inventory.prepend(row({
        {"药品编码", code},
        {"条形码", barcode},
        {"药品名称", name},
        {"分类", payload.value("category").toString("未分类")},
        {"规格", payload.value("specification").toString()},
        {"单位", payload.value("unit").toString("盒")},
        {"售价", payload.value("salePrice").toDouble()},
        {"库存", quantity},
        {"预警库存", payload.value("warningQuantity").toInt(10)},
        {"有效期", payload.value("expiryDate").toString()},
        {"预警原因", inventoryWarningReason(row({{"库存", quantity}, {"预警库存", payload.value("warningQuantity").toInt(10)}, {"有效期", payload.value("expiryDate").toString()}}))},
        {"状态", 1}
    }));
    return makeResult(true, "新药品已入库。");
}

QJsonObject DemoRepository::updateInventory(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString code = payload.value("药品编码").toString();
    for (int i = 0; i < m_inventory.size(); ++i) {
        auto drug = m_inventory.at(i).toObject();
        if (drug.value("药品编码").toString() == code) {
            drug["条形码"] = payload.value("条形码").toString(drug.value("条形码").toString());
            drug["药品名称"] = payload.value("药品名称").toString(drug.value("药品名称").toString());
            drug["分类"] = payload.value("分类").toString(drug.value("分类").toString());
            drug["规格"] = payload.value("规格").toString(drug.value("规格").toString());
            drug["单位"] = payload.value("单位").toString(drug.value("单位").toString());
            drug["售价"] = payload.value("售价").toVariant().toDouble();
            drug["库存"] = payload.value("库存").toVariant().toInt();
            drug["预警库存"] = payload.value("预警库存").toVariant().toInt();
            drug["有效期"] = payload.value("有效期").toString(drug.value("有效期").toString());
            drug["预警原因"] = inventoryWarningReason(drug);
            drug["状态"] = payload.contains("状态") ? payload.value("状态").toVariant().toInt() : 1;
            m_inventory.replace(i, drug);
            return makeResult(true, "药品信息已修改。");
        }
    }
    return makeResult(false, "未找到要修改的药品。");
}

QJsonObject DemoRepository::disableInventory(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString code = payload.value("药品编码").toString();
    for (int i = 0; i < m_inventory.size(); ++i) {
        auto drug = m_inventory.at(i).toObject();
        if (drug.value("药品编码").toString() == code) {
            drug["状态"] = 0;
            m_inventory.replace(i, drug);
            return makeResult(true, "药品已停用。");
        }
    }
    return makeResult(false, "未找到要停用的药品。");
}

QJsonObject DemoRepository::updateBill(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString billNo = payload.value("账单号").toString();
    for (int i = 0; i < m_bills.size(); ++i) {
        auto bill = m_bills.at(i).toObject();
        if (bill.value("账单号").toString() == billNo) {
            bill["患者"] = payload.value("患者").toString(bill.value("患者").toString());
            bill["挂号费"] = payload.value("挂号费").toVariant().toDouble();
            bill["药品费"] = payload.value("药品费").toVariant().toDouble();
            bill["其他费用"] = payload.value("其他费用").toVariant().toDouble();
            bill["合计"] = payload.value("合计").toVariant().toDouble();
            bill["状态"] = payload.value("状态").toString(bill.value("状态").toString());
            m_bills.replace(i, bill);
            return makeResult(true, "账单信息已修改。");
        }
    }
    return makeResult(false, "未找到要修改的账单。");
}

QJsonObject DemoRepository::cancelBill(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString billNo = payload.value("账单号").toString();
    for (int i = 0; i < m_bills.size(); ++i) {
        auto bill = m_bills.at(i).toObject();
        if (bill.value("账单号").toString() == billNo) {
            bill["状态"] = "CANCELLED";
            m_bills.replace(i, bill);
            return makeResult(true, "账单已取消。");
        }
    }
    return makeResult(false, "未找到要取消的账单。");
}

QJsonObject DemoRepository::payBill(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString billNo = payload.value("账单号").toString();
    const QString method = payload.value("支付方式").toString("现金");
    for (int i = 0; i < m_bills.size(); ++i) {
        auto bill = m_bills.at(i).toObject();
        if (bill.value("账单号").toString() == billNo) {
            if (bill.value("状态").toString() == "PAID") {
                return makeResult(true, "该账单已缴费。");
            }
            if (bill.value("状态").toString() == "REFUNDED") {
                return makeResult(false, "该账单已退费，不能再次收费。");
            }
            bill["状态"] = "PAID";
            bill["支付方式"] = method;
            bill["发票号"] = payload.value("发票号").toString(QString("INV%1").arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss")));
            bill["支付时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_bills.replace(i, bill);
            return makeResult(true, "收费成功，已记录支付方式和发票号。");
        }
    }
    return makeResult(false, "未找到要收费的账单。");
}

QJsonObject DemoRepository::refundBill(const QJsonObject& payload)
{
    QMutexLocker locker(&m_mutex);
    const QString billNo = payload.value("账单号").toString();
    for (int i = 0; i < m_bills.size(); ++i) {
        auto bill = m_bills.at(i).toObject();
        if (bill.value("账单号").toString() == billNo) {
            if (bill.value("状态").toString() != "PAID") {
                return makeResult(false, "只有已缴费账单可以退费。");
            }
            const QString registrationNo = bill.value("挂号单号").toString();
            for (const auto& prescriptionItem : m_prescriptions) {
                const auto prescription = prescriptionItem.toObject();
                if (prescription.value("挂号单号").toString() == registrationNo
                    && prescription.value("状态").toString() == "已发药") {
                    m_operationLogs.prepend(row({
                        {"操作人", payload.value("__operatorName").toString("收费员")},
                        {"模块", "收费结算"},
                        {"动作", "退费拦截"},
                        {"内容", QString("账单 %1 存在已发药处方，请先完成药房退药。").arg(billNo)},
                        {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
                    }));
                    return makeResult(false, "该账单存在已发药处方，请先完成药房退药后再退费。");
                }
            }
            bill["状态"] = "REFUNDED";
            bill["退费原因"] = payload.value("退费原因").toString("人工退费");
            bill["退费时间"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_bills.replace(i, bill);
            m_operationLogs.prepend(row({
                {"操作人", payload.value("__operatorName").toString("收费员")},
                {"模块", "收费结算"},
                {"动作", "退费处理"},
                {"内容", "退费成功 " + billNo},
                {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
            }));
            return makeResult(true, "退费成功，账单状态已更新。");
        }
    }
    return makeResult(false, "未找到要退费的账单。");
}

void DemoRepository::appendOperationLog(const QString& operatorName,
                                        const QString& module,
                                        const QString& action,
                                        const QString& content)
{
    QMutexLocker locker(&m_mutex);
    m_operationLogs.prepend(row({
        {"操作人", operatorName.isEmpty() ? "未知用户" : operatorName},
        {"模块", module},
        {"动作", action},
        {"内容", content.left(300)},
        {"操作时间", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    }));
    saveState();
}

QJsonObject DemoRepository::makeResult(bool success, const QString& message) const
{
    QJsonObject result;
    result["success"] = success;
    result["message"] = message;
    if (success) {
        saveState();
    }
    return result;
}

void DemoRepository::ensureCatalogData()
{
    auto ensureDepartment = [this](const QString& name, const QString& location) {
        if (name.trimmed().isEmpty()) {
            return;
        }
        for (const auto& item : m_departments) {
            const auto department = item.toObject();
            if (department.value("科室名称").toString() == name) {
                return;
            }
        }

        m_departments.append(row({
            {"科室编码", QString("CAT%1").arg(m_departments.size() + 1, 3, 10, QChar('0'))},
            {"科室名称", name},
            {"位置", location},
            {"状态", 1}
        }));
    };

    auto hasDoctor = [this](const QString& name) {
        for (const auto& item : m_doctors) {
            const auto doctor = item.toObject();
            if (doctor.value("医生姓名").toString() == name) {
                return true;
            }
        }
        return false;
    };

    for (const auto& seed : catalogDoctorSeeds) {
        const QString category = QString::fromUtf8(seed.category);
        const QString specialty = QString::fromUtf8(seed.specialty);
        const QString clinic = QString::fromUtf8(seed.clinic);
        ensureDepartment(category, "门诊楼");
        ensureDepartment(specialty, category);
        ensureDepartment(clinic, category + "-" + specialty);

        const QString doctorName = QString::fromUtf8(seed.doctor);
        if (hasDoctor(doctorName)) {
            continue;
        }

        m_doctors.append(row({
            {"医生姓名", doctorName},
            {"所属科室", clinic},
            {"职称", QString::fromUtf8(seed.title)},
            {"擅长方向", QString("%1常见病、多发病及%2专病诊疗").arg(specialty, clinic)},
            {"挂号费", seed.fee},
            {"电话", QString::fromUtf8(seed.phone)},
            {"状态", 1}
        }));
    }
}

int DemoRepository::findScheduleIndex(const QString& doctor, const QString& date, const QString& period, const QString& department) const
{
    for (int i = 0; i < m_schedules.size(); ++i) {
        const auto schedule = m_schedules.at(i).toObject();
        const QString schedulePeriod = schedule.value("时段").toString();
        const bool periodMatches = period.trimmed().isEmpty()
            || period == "全天"
            || schedulePeriod == period;
        if (schedule.value("医生").toString() == doctor
            && schedule.value("出诊日期").toString() == date
            && periodMatches
            && (department.trimmed().isEmpty() || schedule.value("科室").toString() == department.trimmed())) {
            return i;
        }
    }
    return -1;
}

QString DemoRepository::stateFilePath() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    dir.mkpath("data");
    return dir.filePath("data/demo-state.json");
}

void DemoRepository::loadState()
{
    QFile file(stateFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject state = document.object();
    if (state.value("version").toInt() < 3) {
        return;
    }
    m_patients = state.value("patients").toArray(m_patients);
    m_departments = state.value("departments").toArray(m_departments);
    m_registrations = state.value("registrations").toArray(m_registrations);
    m_schedules = state.value("schedules").toArray(m_schedules);
    m_doctors = state.value("doctors").toArray(m_doctors);
    m_consultations = state.value("consultations").toArray(m_consultations);
    m_prescriptions = state.value("prescriptions").toArray(m_prescriptions);
    m_examinations = state.value("examinations").toArray(m_examinations);
    m_inventory = state.value("inventory").toArray(m_inventory);
    m_bills = state.value("bills").toArray(m_bills);
    m_statistics = state.value("statistics").toArray(m_statistics);
    m_operationLogs = state.value("operationLogs").toArray(m_operationLogs);
}

void DemoRepository::saveState() const
{
    QJsonObject state;
    state["version"] = 3;
    state["savedAt"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    state["patients"] = m_patients;
    state["departments"] = m_departments;
    state["registrations"] = m_registrations;
    state["schedules"] = m_schedules;
    state["doctors"] = m_doctors;
    state["consultations"] = m_consultations;
    state["prescriptions"] = m_prescriptions;
    state["examinations"] = m_examinations;
    state["inventory"] = m_inventory;
    state["bills"] = m_bills;
    state["statistics"] = m_statistics;
    state["operationLogs"] = m_operationLogs;

    QSaveFile file(stateFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace hospital::server
