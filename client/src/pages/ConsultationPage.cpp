#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageSize>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

QFrame* createMetricCard(const QString& title, QLabel** valueLabel, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("consultMetricCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("consultMetricTitle");
    auto* value = new QLabel("--", card);
    value->setObjectName("consultMetricValue");
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    layout->addStretch();

    if (valueLabel) {
        *valueLabel = value;
    }
    return card;
}

QLabel* createStatusChip(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

struct PrescriptionDraft
{
    QString drug;
    int quantity = 1;
    QString dosage;
    QString frequency;
    int days = 3;
};

constexpr const char* kDecisionFinish = "finish";
constexpr const char* kDecisionPrescription = "prescription";
constexpr const char* kDecisionExamination = "examination";
constexpr const char* kDecisionWaiting = "waiting";

bool fillPrescriptionDraft(QWidget* parent, const QString& registrationNo, PrescriptionDraft* draft)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("开立处方 - " + registrationNo);
    dialog.resize(520, 320);

    auto* form = new QFormLayout(&dialog);
    auto* registrationEdit = new QLineEdit(registrationNo, &dialog);
    registrationEdit->setReadOnly(true);
    auto* drugEdit = new QLineEdit(&dialog);
    auto* quantitySpin = new QSpinBox(&dialog);
    auto* dosageEdit = new QLineEdit(&dialog);
    auto* frequencyEdit = new QLineEdit(&dialog);
    auto* daysSpin = new QSpinBox(&dialog);

    drugEdit->setPlaceholderText("输入药品名称、药品编码或条形码");
    quantitySpin->setRange(1, 999);
    quantitySpin->setValue(1);
    dosageEdit->setPlaceholderText("例如：一次1粒");
    frequencyEdit->setPlaceholderText("例如：每日3次");
    daysSpin->setRange(1, 60);
    daysSpin->setValue(3);

    form->addRow("挂号单号", registrationEdit);
    form->addRow("药品", drugEdit);
    form->addRow("数量", quantitySpin);
    form->addRow("用法用量", dosageEdit);
    form->addRow("频次", frequencyEdit);
    form->addRow("天数", daysSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存处方");
    buttons->button(QDialogButtonBox::Cancel)->setText("暂不开方");
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (drugEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(parent, "信息不完整", "药品不能为空。");
        return false;
    }

    draft->drug = drugEdit->text().trimmed();
    draft->quantity = quantitySpin->value();
    draft->dosage = dosageEdit->text().trimmed();
    draft->frequency = frequencyEdit->text().trimmed();
    draft->days = daysSpin->value();
    return true;
}

QString consultationAdviceFor(const QString& text)
{
    QStringList tips;
    const QString lower = text.toLower();
    auto add = [&tips](const QString& tip) {
        if (!tips.contains(tip)) {
            tips.append(tip);
        }
    };

    if (text.contains("胸痛") || text.contains("胸闷") || text.contains("心悸")) {
        add("风险提示：胸痛/胸闷需优先排除心血管急症，建议心电图、心肌酶/肌钙蛋白，必要时急诊处理。");
        add("可能方向：冠心病、心律失常、心肌炎、肺部疾病等。");
    }
    if (text.contains("发热") || text.contains("咳嗽") || text.contains("咽痛")) {
        add("建议检查：体温、血常规、CRP，咳嗽明显可考虑胸片/肺部影像。");
        add("可能方向：上呼吸道感染、肺部感染、支气管炎等。");
    }
    if (text.contains("腹痛") || text.contains("呕吐") || text.contains("腹泻")) {
        add("建议检查：腹部查体、血常规、腹部彩超，右下腹痛需警惕阑尾炎。");
        add("风险提示：持续剧痛、发热、便血或脱水表现需优先处理。");
    }
    if (text.contains("头痛") || text.contains("头晕") || text.contains("肢体麻木")) {
        add("风险提示：突发剧烈头痛、肢体无力/麻木、言语不清需排除脑血管事件。");
        add("建议检查：血压、神经系统查体，必要时头颅CT/MRI。");
    }
    if (text.contains("关节") || text.contains("骨") || text.contains("腰") || text.contains("腿痛")) {
        add("建议方向：骨科/关节或脊柱相关疾病，必要时X线、CT或MRI。");
    }
    if (lower.contains("青霉素") || text.contains("过敏")) {
        add("用药风险：存在过敏信息，开具抗生素或止痛药前需再次核对过敏史。");
    }

    if (tips.isEmpty()) {
        tips.append("未命中特定高风险关键词。建议完善主诉、既往史、过敏史和体征后再保存诊断。");
    }
    return tips.join("\n");
}

struct MedicalRecordTemplate
{
    QString complaint;
    QString presentIllness;
    QString pastHistory;
    QString physicalSign;
    QString diagnosis;
    QString advice;
};

MedicalRecordTemplate medicalRecordTemplateFor(const QString& name)
{
    if (name == "骨科模板") {
        return {
            "疼痛/活动受限，具体部位、诱因和持续时间待补充。",
            "记录受伤机制、疼痛性质、活动受限程度、麻木无力及既往处理情况。",
            "记录既往骨折、手术、慢性病、用药史及过敏史。",
            "记录局部肿胀、压痛、畸形、活动度、肌力、感觉和末梢循环。",
            "骨关节疼痛待查",
            "建议完善X线/CT/MRI等检查，避免负重，必要时骨科复诊。"
        };
    }
    if (name == "儿科模板") {
        return {
            "发热/咳嗽/腹痛等症状，起病时间和伴随症状待补充。",
            "记录体温峰值、精神食欲、咳喘、呕吐腹泻、皮疹及接触史。",
            "记录出生史、疫苗接种史、既往疾病史、过敏史及家族史。",
            "记录体温、咽部、心肺、腹部、皮肤及神经系统查体。",
            "儿童常见病待查",
            "建议完善血常规/CRP等检查，补液休息，病情变化及时复诊。"
        };
    }
    return {
        "主要症状、持续时间和诱因待补充。",
        "记录起病经过、症状变化、伴随症状、已用药物和疗效。",
        "记录高血压、糖尿病、冠心病等慢性病史、手术史、过敏史。",
        "记录生命体征、心肺腹及相关专科体征。",
        "内科疾病待查",
        "完善相关检查，规律用药，注意休息，按医嘱复诊。"
    };
}

void fillIfEmpty(QTextEdit* edit, const QString& text)
{
    if (edit->toPlainText().trimmed().isEmpty()) {
        edit->setPlainText(text);
    }
}

void applyMedicalRecordTemplate(QTextEdit* complaintEdit,
                                QTextEdit* presentIllnessEdit,
                                QTextEdit* pastHistoryEdit,
                                QTextEdit* physicalSignEdit,
                                QTextEdit* diagnosisEdit,
                                QTextEdit* adviceEdit,
                                const QString& templateName)
{
    const auto recordTemplate = medicalRecordTemplateFor(templateName);
    fillIfEmpty(complaintEdit, recordTemplate.complaint);
    fillIfEmpty(presentIllnessEdit, recordTemplate.presentIllness);
    fillIfEmpty(pastHistoryEdit, recordTemplate.pastHistory);
    fillIfEmpty(physicalSignEdit, recordTemplate.physicalSign);
    fillIfEmpty(diagnosisEdit, recordTemplate.diagnosis);
    fillIfEmpty(adviceEdit, recordTemplate.advice);
}

QString htmlRow(const QString& title, const QString& value)
{
    return "<tr><th>" + title.toHtmlEscaped() + "</th><td>"
        + value.toHtmlEscaped().replace("\n", "<br/>") + "</td></tr>";
}

QString renderMedicalRecordHtml(const QJsonObject& row,
                                const QString& complaint,
                                const QString& presentIllness,
                                const QString& pastHistory,
                                const QString& physicalSign,
                                const QString& icdCode,
                                const QString& diagnosis,
                                const QString& advice,
                                const QString& externalReportHospital,
                                const QString& externalReportType,
                                const QString& externalReportDate,
                                const QString& externalReportSummary,
                                const QString& externalReportConclusion,
                                const QString& externalReportAttachment)
{
    QString html;
    html += "<html><head><meta charset='utf-8'/><style>";
    html += "body{font-family:'Microsoft YaHei',sans-serif;font-size:11pt;color:#1f2933;}";
    html += "h1{text-align:center;font-size:18pt;margin-bottom:18px;}";
    html += "table{width:100%;border-collapse:collapse;}th{width:22%;background:#eef3f7;text-align:left;}";
    html += "th,td{border:1px solid #b8c4d0;padding:8px;vertical-align:top;}";
    html += "</style></head><body><h1>门诊电子病历</h1><table>";
    html += htmlRow("挂号单号", row.value("挂号单号").toString());
    html += htmlRow("患者", row.value("患者").toString());
    html += htmlRow("医生", row.value("医生").toString());
    html += htmlRow("科室", row.value("科室").toString());
    html += htmlRow("就诊日期", row.value("就诊日期").toString());
    html += htmlRow("主诉", complaint);
    html += htmlRow("现病史", presentIllness);
    html += htmlRow("既往史", pastHistory);
    html += htmlRow("体格检查", physicalSign);
    html += htmlRow("ICD编码", icdCode);
    html += htmlRow("诊断", diagnosis);
    html += htmlRow("医嘱", advice);
    html += htmlRow("外院报告医院", externalReportHospital);
    html += htmlRow("外院报告类型", externalReportType);
    html += htmlRow("外院报告日期", externalReportDate);
    html += htmlRow("外院报告摘要", externalReportSummary);
    html += htmlRow("外院报告结论", externalReportConclusion);
    html += htmlRow("外院报告附件", externalReportAttachment);
    html += "</table></body></html>";
    return html;
}

} // namespace

ConsultationPage::ConsultationPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("医生接诊", "已叫号、接诊中、检查完成待复诊的患者在这里处理；医生可选择结束、开药、申请检查或复诊观察。", "consultation", "list", apiClient, parent, 10000)
{
    auto* summaryLayout = new QHBoxLayout();
    summaryLayout->setSpacing(12);
    summaryLayout->addWidget(createMetricCard("待接诊", &m_pendingConsultationLabel, this));
    summaryLayout->addWidget(createMetricCard("接诊中", &m_inConsultationLabel, this));
    summaryLayout->addWidget(createMetricCard("待复诊", &m_reviewCountLabel, this));
    summaryLayout->addWidget(createMetricCard("今日已完成", &m_finishedTodayLabel, this));

    auto* flowBar = new QFrame(this);
    flowBar->setObjectName("consultFlowBar");
    auto* flowLayout = new QHBoxLayout(flowBar);
    flowLayout->setContentsMargins(14, 10, 14, 10);
    flowLayout->setSpacing(10);
    flowLayout->addWidget(createStatusChip("已叫号", "consultChipPending", flowBar));
    flowLayout->addWidget(createStatusChip("接诊中", "consultChipActive", flowBar));
    flowLayout->addWidget(createStatusChip("检查完成待复诊", "consultChipReview", flowBar));
    flowLayout->addStretch();
    auto* flowHint = new QLabel("先在候诊队列叫号，再在这里开始接诊；检查结果回传后会再次进入这里。", flowBar);
    flowHint->setObjectName("consultFlowHint");
    flowLayout->addWidget(flowHint);

    auto* actionBar = new QHBoxLayout();
    auto* hint = new QLabel("操作：候诊队列叫号后开始接诊；需拍片/化验时保存为“检查中”，检查完成后再回到这里复诊判断。", this);
    hint->setStyleSheet("color:#5f7080;");
    m_callButton = new QPushButton("叫号", this);
    m_callButton->setMinimumHeight(36);
    m_callButton->setObjectName("secondaryButton");
    m_startButton = new QPushButton("开始接诊", this);
    m_startButton->setMinimumHeight(36);
    m_startButton->setObjectName("primaryButton");
    actionBar->addWidget(hint);
    actionBar->addStretch();
    actionBar->addWidget(m_callButton);
    actionBar->addWidget(m_startButton);

    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        root->insertLayout(3, summaryLayout);
        root->insertWidget(4, flowBar);
        root->insertLayout(5, actionBar);
    }
    setStyleSheet(styleSheet() + R"(
        QFrame#consultMetricCard {
            background: #ffffff;
            border: 1px solid #d9e6e2;
            border-radius: 10px;
        }
        QLabel#consultMetricTitle {
            color: #5f7080;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#consultMetricValue {
            color: #0f172a;
            font-size: 28px;
            font-weight: 700;
        }
        QFrame#consultFlowBar {
            background: #f7fbfb;
            border: 1px solid #d9e6e2;
            border-radius: 10px;
        }
        QLabel#consultChipPending, QLabel#consultChipActive, QLabel#consultChipReview {
            padding: 6px 12px;
            border-radius: 999px;
            font-weight: 600;
        }
        QLabel#consultChipPending {
            color: #b45309;
            background: #fff7ed;
            border: 1px solid #fed7aa;
        }
        QLabel#consultChipActive {
            color: #0f766e;
            background: #f0fdfa;
            border: 1px solid #99f6e4;
        }
        QLabel#consultChipReview {
            color: #1d4ed8;
            background: #eff6ff;
            border: 1px solid #bfdbfe;
        }
        QLabel#consultFlowHint {
            color: #5f7080;
        }
    )");

    connect(m_callButton, &QPushButton::clicked, this, &ConsultationPage::callSelectedPatient);
    connect(m_startButton, &QPushButton::clicked, this, &ConsultationPage::startConsultation);
    connect(apiClient, &ApiClient::responseReceived, this, &ConsultationPage::onConsultationResponse);
    loadExaminationItems();
}

void ConsultationPage::rowsUpdated(const QJsonArray& rows)
{
    int pendingCount = 0;
    int activeCount = 0;
    int reviewCount = 0;
    int finishedToday = 0;

    for (const auto& item : rows) {
        const QJsonObject row = item.toObject();
        const QString status = row.value("状态").toString();
        if (status == "已叫号") {
            ++pendingCount;
        } else if (status == "接诊中") {
            ++activeCount;
        } else if (status == "检查完成待复诊") {
            ++reviewCount;
        } else if (status == "已接诊") {
            ++finishedToday;
        }
    }

    if (m_pendingConsultationLabel) {
        m_pendingConsultationLabel->setText(QString::number(pendingCount));
    }
    if (m_inConsultationLabel) {
        m_inConsultationLabel->setText(QString::number(activeCount));
    }
    if (m_reviewCountLabel) {
        m_reviewCountLabel->setText(QString::number(reviewCount));
    }
    if (m_finishedTodayLabel) {
        m_finishedTodayLabel->setText(QString::number(finishedToday));
    }
}

void ConsultationPage::loadExaminationItems()
{
    common::Request request;
    request.module = "examination";
    request.action = "items";
    apiClient()->send(request);
}

void ConsultationPage::callSelectedPatient()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择患者", "请先在表格中点击要叫号的患者。");
        return;
    }

    const QString status = row.value("状态").toString();
    if (status == "已接诊") {
        QMessageBox::information(this, "已接诊", "该患者已经完成接诊，不需要再叫号。");
        return;
    }
    if (status == "已叫号") {
        QMessageBox::information(this, "已叫号", "该患者已经叫号，可以直接开始接诊。");
        return;
    }

    common::Request request;
    request.module = "registration";
    request.action = "call";
    request.payload = row;
    apiClient()->send(request);
}

void ConsultationPage::startConsultation()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择患者", "请先在表格中点击一条挂号记录，再开始接诊。");
        return;
    }

    const QString registrationNo = row.value("挂号单号").toString().trimmed();
    if (registrationNo.isEmpty()) {
        QMessageBox::warning(this, "无法接诊", "当前行没有挂号单号，不能保存病历。");
        return;
    }
    if (row.value("状态").toString() == "已接诊") {
        QMessageBox::information(this, "已接诊", "该患者已经完成接诊，请到“患者病历档案”查看或修改病历内容。");
        return;
    }
    const QString status = row.value("状态").toString();
    if (status == "待叫号" || status == "待接诊" || status == "检查中" || status == "检查后候诊") {
        QMessageBox::information(this, "请先叫号", "请先在候诊队列点击“叫号”，检查中的患者请等待结果回传后再开始接诊。");
        return;
    }

    common::Request startRequest;
    startRequest.module = "consultation";
    startRequest.action = "start";
    startRequest.payload["挂号单号"] = registrationNo;
    if (!apiClient()->send(startRequest)) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("开始接诊 - " + row.value("患者").toString());
    const QRect availableGeometry = screen()
        ? screen()->availableGeometry()
        : QRect(0, 0, 1024, 768);
    const int dialogWidth = qMin(980, qMax(680, availableGeometry.width() - 80));
    const int dialogHeight = qMin(720, qMax(520, availableGeometry.height() - 80));
    dialog.resize(dialogWidth, dialogHeight);
    dialog.setMinimumSize(qMin(640, dialogWidth), qMin(460, dialogHeight));

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);
    scrollArea->setWidget(content);

    auto* patientBox = new QGroupBox("患者信息", content);
    auto* patientForm = new QFormLayout(patientBox);
    patientForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    auto* registrationEdit = new QLineEdit(registrationNo, &dialog);
    registrationEdit->setReadOnly(true);
    auto* patientEdit = new QLineEdit(row.value("患者").toString(), &dialog);
    patientEdit->setReadOnly(true);
    auto* doctorEdit = new QLineEdit(row.value("医生").toString(), &dialog);
    doctorEdit->setReadOnly(true);
    auto* templateBox = new QComboBox(&dialog);
    auto* templateButton = new QPushButton("套用模板", &dialog);
    auto* historyButton = new QPushButton("调取历史病历", &dialog);
    auto* printButton = new QPushButton("打印病历", &dialog);
    auto* pdfButton = new QPushButton("导出PDF", &dialog);
    auto* templateLayout = new QHBoxLayout();
    auto* icdBox = new QComboBox(&dialog);
    auto* complaintEdit = new QTextEdit(row.value("主诉").toString(), &dialog);
    auto* presentIllnessEdit = new QTextEdit(row.value("现病史").toString(), &dialog);
    auto* pastHistoryEdit = new QTextEdit(row.value("既往史").toString(), &dialog);
    auto* physicalSignEdit = new QTextEdit(row.value("体格检查").toString(), &dialog);
    auto* diagnosisEdit = new QTextEdit(row.value("诊断").toString(), &dialog);
    auto* adviceEdit = new QTextEdit(row.value("医嘱").toString(), &dialog);
    auto* decisionBox = new QComboBox(&dialog);
    auto* assistantText = new QTextEdit(&dialog);
    auto* examBox = new QGroupBox("检查申请", &dialog);
    auto* examForm = new QFormLayout(examBox);
    auto* examItemBox = new QComboBox(examBox);
    auto* examNoteEdit = new QTextEdit(examBox);
    auto* branchHint = new QLabel(&dialog);
    auto* externalHospitalEdit = new QLineEdit(row.value("外院报告医院").toString(), &dialog);
    auto* externalReportTypeBox = new QComboBox(&dialog);
    auto* externalReportDateEdit = new QDateEdit(&dialog);
    auto* externalReportSummaryEdit = new QTextEdit(row.value("外院报告摘要").toString(), &dialog);
    auto* externalReportConclusionEdit = new QTextEdit(row.value("外院报告结论").toString(), &dialog);
    auto* externalReportAttachmentEdit = new QLineEdit(row.value("外院报告附件").toString(), &dialog);
    auto* importExternalReportButton = new QPushButton("导入外院报告", &dialog);
    auto* browseExternalAttachmentButton = new QPushButton("选择附件", &dialog);

    decisionBox->addItem("就诊完毕（无需检查、无需开药）", kDecisionFinish);
    decisionBox->addItem("就诊完毕并开处方", kDecisionPrescription);
    decisionBox->addItem("申请检查，完成后复诊", kDecisionExamination);
    decisionBox->addItem("复诊后继续观察", kDecisionWaiting);

    assistantText->setReadOnly(true);
    assistantText->setMinimumHeight(78);
    assistantText->setPlaceholderText("点击“辅助诊断提示”后显示关键词建议和风险提示。");
    branchHint->setStyleSheet("color:#5f7080;");
    branchHint->setWordWrap(true);

    templateBox->addItems({"内科模板", "骨科模板", "儿科模板"});
    templateLayout->addWidget(templateBox);
    templateLayout->addWidget(templateButton);
    templateLayout->addWidget(historyButton);
    templateLayout->addStretch();
    templateLayout->addWidget(printButton);
    templateLayout->addWidget(pdfButton);

    icdBox->addItem("请选择ICD诊断", "");
    icdBox->addItem("I10.x00 高血压病", "I10.x00");
    icdBox->addItem("I20.900 冠心病", "I20.900");
    icdBox->addItem("J06.900 急性上呼吸道感染", "J06.900");
    icdBox->addItem("J18.900 肺部感染", "J18.900");
    icdBox->addItem("K35.900 急性阑尾炎", "K35.900");
    icdBox->addItem("R10.401 急性腹痛待查", "R10.401");
    icdBox->addItem("M54.500 腰痛", "M54.500");
    icdBox->addItem("S93.401 踝关节扭伤", "S93.401");
    icdBox->addItem("R50.900 发热待查", "R50.900");
    const QString existingIcdCode = row.value("ICD编码").toString();
    const int icdIndex = icdBox->findData(existingIcdCode);
    if (icdIndex > 0) {
        icdBox->setCurrentIndex(icdIndex);
    } else if (!existingIcdCode.isEmpty()) {
        icdBox->addItem(existingIcdCode + " " + row.value("诊断").toString(), existingIcdCode);
        icdBox->setCurrentIndex(icdBox->count() - 1);
    }

    externalReportTypeBox->addItems({"未导入", "CT", "DR/X线", "MRI", "超声", "血常规", "生化检验", "心电图", "病理", "其他"});
    const QString existingExternalType = row.value("外院报告类型").toString().trimmed();
    const int externalTypeIndex = externalReportTypeBox->findText(existingExternalType);
    if (externalTypeIndex >= 0) {
        externalReportTypeBox->setCurrentIndex(externalTypeIndex);
    }
    externalReportDateEdit->setCalendarPopup(true);
    externalReportDateEdit->setDisplayFormat("yyyy-MM-dd");
    const QDate existingExternalDate = QDate::fromString(row.value("外院报告日期").toString().left(10), "yyyy-MM-dd");
    externalReportDateEdit->setDate(existingExternalDate.isValid() ? existingExternalDate : QDate::currentDate());

    complaintEdit->setPlaceholderText("例如：右下腹疼痛一天，伴轻微恶心。");
    presentIllnessEdit->setPlaceholderText("按时间顺序记录症状发生、发展、伴随症状和已处理情况。");
    pastHistoryEdit->setPlaceholderText("记录既往病史、手术史、用药史、过敏史和家族史。");
    physicalSignEdit->setPlaceholderText("记录生命体征、专科查体和阳性/阴性体征。");
    diagnosisEdit->setPlaceholderText("例如：急性腹痛待查。");
    adviceEdit->setPlaceholderText("例如：完善血常规、胸片或腹部彩超，检查结果回报后复诊。");
    auto itemBoxCurrentName = [examItemBox]() {
        const QString dataName = examItemBox->currentData().toString().trimmed();
        return dataName.isEmpty() ? examItemBox->currentText().trimmed() : dataName;
    };
    for (const auto& item : m_examinationItems) {
        const auto object = item.toObject();
        const QString name = object.value("检查项目").toString().trimmed();
        if (!name.isEmpty() && object.value("状态").toString() != "停用") {
            examItemBox->addItem(QString("%1（%2元）").arg(name).arg(object.value("单价").toVariant().toDouble()), name);
        }
    }
    if (examItemBox->count() == 0) {
        examItemBox->addItem("CT", "CT");
        examItemBox->addItem("X线", "X线");
        examItemBox->addItem("血常规", "血常规");
        examItemBox->addItem("腹部彩超", "腹部彩超");
    }
    examItemBox->setEditable(true);
    examItemBox->lineEdit()->setPlaceholderText("从检查项目字典选择，例如 CT、X线、血常规");
    examNoteEdit->setPlaceholderText("填写检查目的、部位、注意事项或需要重点排查的问题。");
    externalHospitalEdit->setPlaceholderText("例如：市人民医院、儿童医院");
    externalReportSummaryEdit->setPlaceholderText("记录外院报告关键指标、影像所见或异常项目。");
    externalReportConclusionEdit->setPlaceholderText("记录外院报告结论，供本次诊疗参考，不替代本院检查闭环。");
    externalReportAttachmentEdit->setPlaceholderText("选择外院 PDF、图片或文本文档路径。");
    complaintEdit->setMinimumHeight(58);
    presentIllnessEdit->setMinimumHeight(82);
    pastHistoryEdit->setMinimumHeight(68);
    physicalSignEdit->setMinimumHeight(68);
    diagnosisEdit->setMinimumHeight(62);
    adviceEdit->setMinimumHeight(72);
    examNoteEdit->setMinimumHeight(70);
    externalReportSummaryEdit->setMinimumHeight(86);
    externalReportConclusionEdit->setMinimumHeight(86);
    examForm->addRow("检查项目", examItemBox);
    examForm->addRow("申请说明", examNoteEdit);

    patientForm->addRow("挂号单号", registrationEdit);
    patientForm->addRow("患者", patientEdit);
    patientForm->addRow("医生", doctorEdit);
    patientForm->addRow("病历模板", templateLayout);
    patientForm->addRow("ICD诊断", icdBox);

    auto* tabs = new QTabWidget(content);
    auto* recordTab = new QWidget(tabs);
    auto* recordForm = new QFormLayout(recordTab);
    recordForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    recordForm->addRow("主诉", complaintEdit);
    recordForm->addRow("现病史", presentIllnessEdit);
    recordForm->addRow("既往史", pastHistoryEdit);
    recordForm->addRow("体格检查", physicalSignEdit);
    recordForm->addRow("诊断结果", diagnosisEdit);
    recordForm->addRow("医嘱", adviceEdit);

    auto* workflowTab = new QWidget(tabs);
    auto* workflowForm = new QFormLayout(workflowTab);
    workflowForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    workflowForm->addRow("本次处理结果", decisionBox);
    workflowForm->addRow("流程提示", branchHint);
    workflowForm->addRow(examBox);
    workflowForm->addRow("辅助提示", assistantText);

    auto* externalTab = new QWidget(tabs);
    auto* externalForm = new QFormLayout(externalTab);
    externalForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    auto* externalActionLayout = new QHBoxLayout();
    externalActionLayout->addWidget(importExternalReportButton);
    externalActionLayout->addStretch();
    auto* externalAttachmentLayout = new QHBoxLayout();
    externalAttachmentLayout->addWidget(externalReportAttachmentEdit, 1);
    externalAttachmentLayout->addWidget(browseExternalAttachmentButton);
    externalForm->addRow("外院报告", externalActionLayout);
    externalForm->addRow("来源医院", externalHospitalEdit);
    externalForm->addRow("报告类型", externalReportTypeBox);
    externalForm->addRow("报告日期", externalReportDateEdit);
    externalForm->addRow("报告摘要", externalReportSummaryEdit);
    externalForm->addRow("报告结论", externalReportConclusionEdit);
    externalForm->addRow("附件路径", externalAttachmentLayout);

    tabs->addTab(recordTab, "病历编辑");
    tabs->addTab(workflowTab, "检查与处方");
    tabs->addTab(externalTab, "外院资料");
    contentLayout->addWidget(patientBox);
    contentLayout->addWidget(tabs);
    rootLayout->addWidget(scrollArea, 1);

    auto* buttons = new QDialogButtonBox(&dialog);
    auto* assistButton = buttons->addButton("辅助诊断提示", QDialogButtonBox::ActionRole);
    auto* saveButton = buttons->addButton("按选择保存", QDialogButtonBox::AcceptRole);
    auto* cancelButton = buttons->addButton("取消", QDialogButtonBox::RejectRole);
    rootLayout->addWidget(buttons);

    const auto updateDecisionUi = [decisionBox, examBox, branchHint, adviceEdit]() {
        const QString decision = decisionBox->currentData().toString();
        const bool needsExam = decision == kDecisionExamination;
        examBox->setVisible(needsExam);
        if (decision == kDecisionFinish) {
            branchHint->setText("适合检查后无明显异常、症状轻微或无需用药的情况；保存后患者从候诊队列移出，病历档案可查。");
        } else if (decision == kDecisionPrescription) {
            branchHint->setText("保存病历后继续填写处方；药房审核、发药和收费结算会沿用同一挂号单。");
        } else if (decision == kDecisionExamination) {
            branchHint->setText("保存病历并开立检查单，患者状态变为“检查中”；检查结果录入后医生在本页再次接诊。");
            if (adviceEdit->toPlainText().trimmed().isEmpty()) {
                adviceEdit->setPlainText("先完成检查，结果回报后复诊。");
            }
        } else {
            branchHint->setText("用于检查后仍需观察、等待补充结果或暂不结束的情况；保存后患者进入检查中，结果回传后再复诊。");
        }
    };
    updateDecisionUi();
    connect(decisionBox, &QComboBox::currentIndexChanged, &dialog, updateDecisionUi);

    connect(templateButton, &QPushButton::clicked, &dialog, [=]() {
        applyMedicalRecordTemplate(complaintEdit,
                                   presentIllnessEdit,
                                   pastHistoryEdit,
                                   physicalSignEdit,
                                   diagnosisEdit,
                                   adviceEdit,
                                   templateBox->currentText());
    });
    connect(icdBox, &QComboBox::currentIndexChanged, &dialog, [icdBox, diagnosisEdit]() {
        const QString code = icdBox->currentData().toString();
        if (code.isEmpty()) {
            return;
        }
        QString diagnosisText = icdBox->currentText();
        if (diagnosisText.startsWith(code)) {
            diagnosisText = diagnosisText.mid(code.size()).trimmed();
        }
        if (!diagnosisText.isEmpty()) {
            diagnosisEdit->setPlainText(diagnosisText);
        }
    });
    connect(historyButton, &QPushButton::clicked, &dialog, [row,
                                                            complaintEdit,
                                                            presentIllnessEdit,
                                                            pastHistoryEdit,
                                                            physicalSignEdit,
                                                            diagnosisEdit,
                                                            adviceEdit,
                                                            icdBox,
                                                            externalHospitalEdit,
                                                            externalReportTypeBox,
                                                            externalReportDateEdit,
                                                            externalReportSummaryEdit,
                                                            externalReportConclusionEdit,
                                                            externalReportAttachmentEdit,
                                                            &dialog]() {
        const bool hasHistory = !row.value("主诉").toString().trimmed().isEmpty()
            || !row.value("现病史").toString().trimmed().isEmpty()
            || !row.value("既往史").toString().trimmed().isEmpty()
            || !row.value("体格检查").toString().trimmed().isEmpty()
            || !row.value("诊断").toString().trimmed().isEmpty()
            || !row.value("医嘱").toString().trimmed().isEmpty()
            || !row.value("外院报告结论").toString().trimmed().isEmpty();
        if (!hasHistory) {
            QMessageBox::information(&dialog, "暂无历史病历", "当前患者在本次列表中还没有可调取的历史病历内容。");
            return;
        }
        complaintEdit->setPlainText(row.value("主诉").toString());
        presentIllnessEdit->setPlainText(row.value("现病史").toString());
        pastHistoryEdit->setPlainText(row.value("既往史").toString());
        physicalSignEdit->setPlainText(row.value("体格检查").toString());
        const int historyIcdIndex = icdBox->findData(row.value("ICD编码").toString());
        if (historyIcdIndex >= 0) {
            icdBox->setCurrentIndex(historyIcdIndex);
        }
        diagnosisEdit->setPlainText(row.value("诊断").toString());
        adviceEdit->setPlainText(row.value("医嘱").toString());
        externalHospitalEdit->setText(row.value("外院报告医院").toString());
        const int historyExternalTypeIndex = externalReportTypeBox->findText(row.value("外院报告类型").toString());
        if (historyExternalTypeIndex >= 0) {
            externalReportTypeBox->setCurrentIndex(historyExternalTypeIndex);
        }
        const QDate historyExternalDate = QDate::fromString(row.value("外院报告日期").toString().left(10), "yyyy-MM-dd");
        if (historyExternalDate.isValid()) {
            externalReportDateEdit->setDate(historyExternalDate);
        }
        externalReportSummaryEdit->setPlainText(row.value("外院报告摘要").toString());
        externalReportConclusionEdit->setPlainText(row.value("外院报告结论").toString());
        externalReportAttachmentEdit->setText(row.value("外院报告附件").toString());
    });
    const auto chooseExternalReportFile = [=, &dialog]() {
        const QString fileName = QFileDialog::getOpenFileName(
            &dialog,
            "导入外院报告",
            QString(),
            "报告文件 (*.pdf *.png *.jpg *.jpeg *.bmp *.txt *.doc *.docx);;所有文件 (*.*)");
        if (fileName.isEmpty()) {
            return;
        }
        externalReportAttachmentEdit->setText(fileName);
        if (externalReportTypeBox->currentText() == "未导入") {
            externalReportTypeBox->setCurrentText("其他");
        }
        if (externalReportSummaryEdit->toPlainText().trimmed().isEmpty()) {
            QFileInfo fileInfo(fileName);
            externalReportSummaryEdit->setPlainText("已导入外院报告附件：" + fileInfo.fileName());
        }
    };
    connect(importExternalReportButton, &QPushButton::clicked, &dialog, chooseExternalReportFile);
    connect(browseExternalAttachmentButton, &QPushButton::clicked, &dialog, chooseExternalReportFile);
    auto renderCurrentRecord = [=]() {
        return renderMedicalRecordHtml(row,
                                       complaintEdit->toPlainText().trimmed(),
                                       presentIllnessEdit->toPlainText().trimmed(),
                                       pastHistoryEdit->toPlainText().trimmed(),
                                       physicalSignEdit->toPlainText().trimmed(),
                                       icdBox->currentData().toString(),
                                       diagnosisEdit->toPlainText().trimmed(),
                                       adviceEdit->toPlainText().trimmed(),
                                       externalHospitalEdit->text().trimmed(),
                                       externalReportTypeBox->currentText() == "未导入" ? QString() : externalReportTypeBox->currentText(),
                                       externalReportDateEdit->date().toString("yyyy-MM-dd"),
                                       externalReportSummaryEdit->toPlainText().trimmed(),
                                       externalReportConclusionEdit->toPlainText().trimmed(),
                                       externalReportAttachmentEdit->text().trimmed());
    };
    connect(printButton, &QPushButton::clicked, &dialog, [=, &dialog]() {
        QTextDocument document;
        document.setHtml(renderCurrentRecord());
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageSize(QPageSize(QPageSize::A4));
        QPrintDialog printDialog(&printer, &dialog);
        printDialog.setWindowTitle("打印病历");
        if (printDialog.exec() == QDialog::Accepted) {
            document.print(&printer);
        }
    });
    connect(pdfButton, &QPushButton::clicked, &dialog, [=, &dialog]() {
        QString fileName = QFileDialog::getSaveFileName(&dialog, "导出PDF", registrationNo + "-门诊病历.pdf", "PDF 文件 (*.pdf)");
        if (fileName.isEmpty()) {
            return;
        }
        if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
            fileName += ".pdf";
        }
        QTextDocument document;
        document.setHtml(renderCurrentRecord());
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setOutputFileName(fileName);
        document.print(&printer);
        QMessageBox::information(&dialog, "导出PDF", "病历PDF已导出。");
    });

    connect(assistButton, &QPushButton::clicked, &dialog, [complaintEdit, diagnosisEdit, adviceEdit, assistantText]() {
        const QString text = complaintEdit->toPlainText() + "\n" + diagnosisEdit->toPlainText() + "\n" + adviceEdit->toPlainText();
        assistantText->setPlainText(consultationAdviceFor(text));
    });
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    const auto validateConsultationDraft = [&]() {
        const QString complaint = complaintEdit->toPlainText().trimmed();
        const QString diagnosis = diagnosisEdit->toPlainText().trimmed();
        const QString advice = adviceEdit->toPlainText().trimmed();
        const QString decision = decisionBox->currentData().toString();
        const bool backToWaiting = decision == kDecisionExamination || decision == kDecisionWaiting;
        if (complaint.isEmpty() || advice.isEmpty()) {
            tabs->setCurrentIndex(0);
            QMessageBox::warning(&dialog, "信息不完整", "主诉和医嘱/处理意见都需要填写。");
            return false;
        }
        if (!backToWaiting && diagnosis.isEmpty()) {
            tabs->setCurrentIndex(0);
            QMessageBox::warning(&dialog, "信息不完整", "诊断完成时需要填写诊断结果；如果需要拍片、化验或复诊观察，请在“本次处理结果”里选择候诊分支。");
            return false;
        }
        if (decision == kDecisionExamination && itemBoxCurrentName().isEmpty()) {
            tabs->setCurrentIndex(1);
            QMessageBox::warning(&dialog, "信息不完整", "选择申请检查时，检查项目不能为空。");
            return false;
        }
        return true;
    };

    connect(saveButton, &QPushButton::clicked, &dialog, [&dialog, validateConsultationDraft]() {
        if (validateConsultationDraft()) {
            dialog.accept();
        }
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString complaint = complaintEdit->toPlainText().trimmed();
    const QString presentIllness = presentIllnessEdit->toPlainText().trimmed();
    const QString pastHistory = pastHistoryEdit->toPlainText().trimmed();
    const QString physicalSign = physicalSignEdit->toPlainText().trimmed();
    const QString icdCode = icdBox->currentData().toString();
    const QString diagnosis = diagnosisEdit->toPlainText().trimmed();
    const QString advice = adviceEdit->toPlainText().trimmed();
    const QString externalReportHospital = externalHospitalEdit->text().trimmed();
    const QString externalReportType = externalReportTypeBox->currentText() == "未导入" ? QString() : externalReportTypeBox->currentText();
    const QString externalReportDate = externalReportType.isEmpty() && externalReportHospital.isEmpty()
        && externalReportSummaryEdit->toPlainText().trimmed().isEmpty()
        && externalReportConclusionEdit->toPlainText().trimmed().isEmpty()
        && externalReportAttachmentEdit->text().trimmed().isEmpty()
        ? QString()
        : externalReportDateEdit->date().toString("yyyy-MM-dd");
    const QString externalReportSummary = externalReportSummaryEdit->toPlainText().trimmed();
    const QString externalReportConclusion = externalReportConclusionEdit->toPlainText().trimmed();
    const QString externalReportAttachment = externalReportAttachmentEdit->text().trimmed();
    const QString decision = decisionBox->currentData().toString();
    const bool backToWaiting = decision == kDecisionExamination || decision == kDecisionWaiting;
    bool openPrescription = decision == kDecisionPrescription;
    const QString examItem = itemBoxCurrentName();
    const QString examNote = examNoteEdit->toPlainText().trimmed();

    PrescriptionDraft prescription;
    if (openPrescription && !fillPrescriptionDraft(this, registrationNo, &prescription)) {
        openPrescription = false;
    }

    m_pendingExamRequest = {};
    m_pendingPrescriptionRequest = {};

    common::Request request;
    request.module = "consultation";
    request.action = backToWaiting ? "saveWaiting" : "save";
    request.payload["挂号单号"] = registrationNo;
    request.payload["主诉"] = complaint;
    request.payload["现病史"] = presentIllness;
    request.payload["既往史"] = pastHistory;
    request.payload["体格检查"] = physicalSign;
    request.payload["ICD编码"] = icdCode;
    request.payload["诊断"] = diagnosis;
    request.payload["医嘱"] = advice;
    request.payload["外院报告医院"] = externalReportHospital;
    request.payload["外院报告类型"] = externalReportType;
    request.payload["外院报告日期"] = externalReportDate;
    request.payload["外院报告摘要"] = externalReportSummary;
    request.payload["外院报告结论"] = externalReportConclusion;
    request.payload["外院报告附件"] = externalReportAttachment;
    apiClient()->send(request);

    if (decision == kDecisionExamination) {
        common::Request examRequest;
        examRequest.module = "examination";
        examRequest.action = "create";
        examRequest.payload["挂号单号"] = registrationNo;
        examRequest.payload["检查项目"] = examItem;
        examRequest.payload["申请说明"] = examNote.isEmpty() ? advice : examNote;
        m_pendingExamRequest = examRequest.payload;
    }

    if (openPrescription) {
        common::Request prescriptionRequest;
        prescriptionRequest.module = "prescription";
        prescriptionRequest.action = "create";
        prescriptionRequest.payload["挂号单号"] = registrationNo;
        prescriptionRequest.payload["药品名称"] = prescription.drug;
        prescriptionRequest.payload["数量"] = prescription.quantity;
        prescriptionRequest.payload["用法用量"] = prescription.dosage;
        prescriptionRequest.payload["频次"] = prescription.frequency;
        prescriptionRequest.payload["天数"] = prescription.days;
        m_pendingPrescriptionRequest = prescriptionRequest.payload;
    }
}

void ConsultationPage::onConsultationResponse(const common::Response& response)
{
    if (response.data.value("module").toString() == "examination"
        && response.data.value("action").toString() == "items"
        && response.success) {
        m_examinationItems = response.data.value("rows").toArray();
        return;
    }

    if (response.data.value("module").toString() != "consultation") {
        return;
    }

    const QString action = response.data.value("action").toString();
    if (action != "save" && action != "saveWaiting") {
        return;
    }

    if (!response.success) {
        m_pendingExamRequest = {};
        m_pendingPrescriptionRequest = {};
        return;
    }

    if (!m_pendingExamRequest.isEmpty()) {
        common::Request examRequest;
        examRequest.module = "examination";
        examRequest.action = "create";
        examRequest.payload = m_pendingExamRequest;
        m_pendingExamRequest = {};
        apiClient()->send(examRequest);
    }

    if (!m_pendingPrescriptionRequest.isEmpty()) {
        common::Request prescriptionRequest;
        prescriptionRequest.module = "prescription";
        prescriptionRequest.action = "create";
        prescriptionRequest.payload = m_pendingPrescriptionRequest;
        m_pendingPrescriptionRequest = {};
        apiClient()->send(prescriptionRequest);
    }
}

} // namespace hospital::client
