#include "client/DepartmentCatalog.h"

#include <QVector>

namespace hospital::client::DepartmentCatalog {
namespace {

struct SpecialtyNode
{
    QString name;
    QStringList clinics;
};

struct CategoryNode
{
    QString name;
    QVector<SpecialtyNode> specialties;
};

const QVector<CategoryNode>& tree()
{
    static const QVector<CategoryNode> data = {
        {"内科门诊", {
            {"心血管内科", {"心血管内科诊室", "高血压门诊", "冠心病门诊"}},
            {"血液内科", {"血液内科诊室", "儿童血液诊室", "贫血门诊", "骨髓瘤门诊"}},
            {"肾内科", {"肾内科诊室", "慢性肾病门诊", "血液透析门诊"}},
            {"呼吸内科", {"呼吸内科诊室", "哮喘门诊", "肺部感染门诊"}},
            {"消化内科", {"消化内科诊室", "胃肠门诊", "肝病门诊"}},
            {"内分泌科", {"内分泌科诊室", "糖尿病门诊", "甲状腺门诊"}},
            {"神经内科", {"神经内科诊室", "头痛门诊", "脑卒中随访门诊"}},
            {"风湿免疫科", {"风湿免疫科诊室", "类风湿门诊", "痛风门诊"}},
            {"老年医学科", {"老年医学科诊室", "老年慢病门诊", "综合评估门诊"}}
        }},
        {"外科门诊", {
            {"普外科", {"普外科诊室", "胃肠外科门诊", "肝胆外科门诊"}},
            {"骨科", {"骨科诊室", "关节门诊", "脊柱门诊"}},
            {"泌尿外科", {"泌尿外科诊室", "结石门诊", "前列腺门诊"}},
            {"神经外科", {"神经外科诊室", "颅脑外伤门诊"}},
            {"胸外科", {"胸外科诊室", "肺结节门诊"}}
        }},
        {"儿科门诊", {
            {"儿科普通", {"儿科普通诊室", "儿童发热门诊"}},
            {"儿童血液", {"儿童血液专病门诊", "儿童贫血门诊"}},
            {"儿童呼吸", {"儿童呼吸诊室", "儿童哮喘门诊"}},
            {"儿童消化", {"儿童消化诊室", "儿童腹痛门诊"}}
        }},
        {"妇产科门诊", {
            {"妇科", {"妇科诊室", "宫颈疾病门诊", "月经病门诊"}},
            {"产科", {"产科诊室", "孕期保健门诊", "高危妊娠门诊"}}
        }},
        {"眼耳鼻喉门诊", {
            {"眼科", {"眼科诊室", "视光门诊", "白内障门诊"}},
            {"耳鼻喉科", {"耳鼻喉科诊室", "鼻炎门诊", "咽喉门诊"}}
        }},
        {"口腔科门诊", {
            {"口腔内科", {"口腔内科诊室", "牙体牙髓门诊"}},
            {"口腔修复科", {"口腔修复诊室", "种植修复门诊"}}
        }},
        {"皮肤科门诊", {
            {"皮肤科", {"皮肤科诊室", "湿疹门诊", "痤疮门诊"}}
        }},
        {"中医科门诊", {
            {"中医内科", {"中医内科诊室", "失眠调理门诊", "针灸门诊"}},
            {"中医妇科", {"中医妇科诊室", "月经调理门诊"}},
            {"康复理疗科", {"康复理疗诊室", "颈肩腰腿痛门诊"}}
        }}
    };
    return data;
}

} // namespace

QStringList categories()
{
    QStringList result;
    for (const auto& category : tree()) {
        result.append(category.name);
    }
    return result;
}

QStringList specialties(const QString& category)
{
    for (const auto& categoryNode : tree()) {
        if (categoryNode.name == category) {
            QStringList result;
            for (const auto& specialty : categoryNode.specialties) {
                result.append(specialty.name);
            }
            return result;
        }
    }
    return {};
}

QStringList clinics(const QString& category, const QString& specialty)
{
    for (const auto& categoryNode : tree()) {
        if (categoryNode.name != category) {
            continue;
        }
        for (const auto& specialtyNode : categoryNode.specialties) {
            if (specialtyNode.name == specialty) {
                return specialtyNode.clinics;
            }
        }
    }
    return {};
}

QStringList leavesForCategory(const QString& category)
{
    QStringList result;
    for (const auto& specialty : specialties(category)) {
        result.append(clinics(category, specialty));
    }
    return result;
}

QString categoryFor(const QString& department)
{
    for (const auto& categoryNode : tree()) {
        if (categoryNode.name == department) {
            return categoryNode.name;
        }
        for (const auto& specialtyNode : categoryNode.specialties) {
            if (specialtyNode.name == department || specialtyNode.clinics.contains(department)) {
                return categoryNode.name;
            }
        }
    }
    return {};
}

QString specialtyFor(const QString& department)
{
    for (const auto& categoryNode : tree()) {
        for (const auto& specialtyNode : categoryNode.specialties) {
            if (specialtyNode.name == department || specialtyNode.clinics.contains(department)) {
                return specialtyNode.name;
            }
        }
    }
    return {};
}

QString clinicFor(const QString& department)
{
    for (const auto& categoryNode : tree()) {
        for (const auto& specialtyNode : categoryNode.specialties) {
            if (specialtyNode.clinics.contains(department)) {
                return department;
            }
            if (specialtyNode.name == department) {
                return specialtyNode.clinics.isEmpty() ? department : specialtyNode.clinics.first();
            }
        }
    }
    return department;
}

QString firstClinic(const QString& category, const QString& specialty)
{
    const auto result = clinics(category, specialty);
    return result.isEmpty() ? specialty : result.first();
}

QString recommendedClinicForSymptoms(const QString& text)
{
    const QString symptoms = text.trimmed();
    if (symptoms.isEmpty()) {
        return "内科门诊";
    }

    if (symptoms.contains("儿童") || symptoms.contains("孩子") || symptoms.contains("小孩") || symptoms.contains("婴儿")) {
        if (symptoms.contains("咳") || symptoms.contains("喘") || symptoms.contains("肺") || symptoms.contains("呼吸")) {
            return "儿童呼吸诊室";
        }
        if (symptoms.contains("腹痛") || symptoms.contains("肚子") || symptoms.contains("腹泻") || symptoms.contains("呕吐")) {
            return "儿童腹痛门诊";
        }
        if (symptoms.contains("贫血") || symptoms.contains("血液") || symptoms.contains("血小板")) {
            return "儿童血液诊室";
        }
        return "儿科普通诊室";
    }

    struct Rule
    {
        QString clinic;
        QStringList keywords;
    };

    const QVector<Rule> rules = {
        {"心血管内科诊室", {"胸痛", "胸闷", "心慌", "心悸", "心跳", "高血压", "冠心病", "血压高"}},
        {"血液内科诊室", {"贫血", "白细胞", "血小板", "淋巴", "血液", "出血点", "紫癜"}},
        {"肾内科诊室", {"肾", "尿蛋白", "血尿", "少尿", "水肿", "透析", "腰酸伴尿"}},
        {"呼吸内科诊室", {"咳嗽", "咳痰", "哮喘", "气喘", "呼吸", "肺", "发热", "发烧", "咽痛"}},
        {"消化内科诊室", {"胃", "腹泻", "拉肚子", "消化", "反酸", "腹痛", "肚子痛", "恶心", "呕吐", "便秘", "便血"}},
        {"内分泌科诊室", {"糖尿病", "血糖", "甲状腺", "内分泌", "口渴", "多饮", "多尿"}},
        {"神经内科诊室", {"头痛", "头晕", "眩晕", "手麻", "脚麻", "抽搐", "癫痫", "失眠", "记忆力", "脑梗"}},
        {"风湿免疫科诊室", {"类风湿", "风湿", "痛风", "红斑狼疮", "关节肿", "晨僵"}},
        {"老年医学科诊室", {"老人", "老年", "多病", "慢病", "综合评估"}},
        {"骨科诊室", {"骨头疼", "骨痛", "骨折", "骨裂", "关节痛", "关节疼", "腰疼", "腰痛", "腿疼", "腿痛", "膝盖", "肩痛", "颈椎", "脊柱", "扭伤", "崴脚"}},
        {"普外科诊室", {"伤口", "外伤", "刀伤", "手术", "阑尾", "疝气", "包块", "乳腺", "胆囊"}},
        {"泌尿外科诊室", {"尿痛", "尿频", "尿急", "结石", "前列腺", "排尿", "尿不出"}},
        {"神经外科诊室", {"颅脑外伤", "头部外伤", "脑外伤", "脑肿瘤"}},
        {"胸外科诊室", {"肺结节", "胸部外伤", "气胸"}},
        {"妇科诊室", {"月经", "痛经", "白带", "妇科", "阴道", "宫颈", "盆腔"}},
        {"产科诊室", {"怀孕", "孕", "产检", "妊娠", "胎动"}},
        {"眼科诊室", {"眼", "视力", "近视", "白内障", "眼红", "眼痛", "流泪"}},
        {"耳鼻喉科诊室", {"耳", "鼻", "喉", "鼻炎", "鼻塞", "流鼻涕", "咽喉", "听力"}},
        {"口腔内科诊室", {"牙", "牙疼", "牙痛", "口腔", "牙龈", "蛀牙"}},
        {"皮肤科诊室", {"皮肤", "皮疹", "湿疹", "痤疮", "瘙痒", "过敏", "荨麻疹"}},
        {"中医内科诊室", {"调理", "中医", "体虚", "乏力", "睡眠差"}},
        {"康复理疗诊室", {"康复", "理疗", "针灸", "颈肩腰腿痛", "肌肉酸痛"}}
    };

    QString bestClinic = "内科门诊";
    int bestScore = 0;
    for (const auto& rule : rules) {
        int score = 0;
        for (const QString& keyword : rule.keywords) {
            if (symptoms.contains(keyword)) {
                score += keyword.size();
            }
        }
        if (score > bestScore) {
            bestScore = score;
            bestClinic = rule.clinic;
        }
    }

    return bestClinic;
}

} // namespace hospital::client::DepartmentCatalog
