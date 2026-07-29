#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace hospital::client {

PrescriptionPage::PrescriptionPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("处方管理", "开立处方、添加药品明细、计算处方金额并提交审核。", "prescription", "list", apiClient, parent, 10000)
{
    auto* actionBar = new QHBoxLayout();
    auto* createButton = new QPushButton("开处方", this);
    auto* reviewButton = new QPushButton("审核处方", this);
    auto* dispenseButton = new QPushButton("确认发药", this);
    auto* returnButton = new QPushButton("退药入库", this);
    createButton->setObjectName("primaryButton");
    reviewButton->setObjectName("warningButton");
    dispenseButton->setObjectName("primaryButton");
    returnButton->setObjectName("warningButton");
    createButton->setMinimumHeight(36);
    createButton->setStyleSheet("font-weight:600;");
    actionBar->addStretch();
    actionBar->addWidget(createButton);
    actionBar->addWidget(reviewButton);
    actionBar->addWidget(dispenseButton);
    actionBar->addWidget(returnButton);

    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        root->insertLayout(3, actionBar);
    }

    connect(createButton, &QPushButton::clicked, this, &PrescriptionPage::createPrescription);
    connect(reviewButton, &QPushButton::clicked, this, &PrescriptionPage::reviewPrescription);
    connect(dispenseButton, &QPushButton::clicked, this, &PrescriptionPage::dispensePrescription);
    connect(returnButton, &QPushButton::clicked, this, &PrescriptionPage::returnPrescription);
}

void PrescriptionPage::createPrescription()
{
    QDialog dialog(this);
    dialog.setWindowTitle("开立处方");
    dialog.resize(520, 360);

    auto* form = new QFormLayout(&dialog);
    auto* registrationEdit = new QLineEdit(&dialog);
    auto* drugEdit = new QLineEdit(&dialog);
    auto* quantitySpin = new QSpinBox(&dialog);
    auto* dosageEdit = new QLineEdit(&dialog);
    auto* frequencyEdit = new QLineEdit(&dialog);
    auto* daysSpin = new QSpinBox(&dialog);
    auto* allergyEdit = new QLineEdit(&dialog);
    auto* conditionEdit = new QLineEdit(&dialog);

    registrationEdit->setPlaceholderText("从医生接诊或患者病历档案复制挂号单号");
    drugEdit->setPlaceholderText("可输入药品名称、药品编码或条形码");
    quantitySpin->setRange(1, 999);
    quantitySpin->setValue(1);
    dosageEdit->setPlaceholderText("例如：一次1粒");
    frequencyEdit->setPlaceholderText("例如：每日3次");
    allergyEdit->setPlaceholderText("例如：青霉素过敏");
    conditionEdit->setPlaceholderText("例如：儿童、老人、肾功能异常");
    daysSpin->setRange(1, 60);
    daysSpin->setValue(3);

    form->addRow("挂号单号", registrationEdit);
    form->addRow("药品", drugEdit);
    form->addRow("数量", quantitySpin);
    form->addRow("用法用量", dosageEdit);
    form->addRow("频次", frequencyEdit);
    form->addRow("天数", daysSpin);
    form->addRow("过敏史", allergyEdit);
    form->addRow("患者情况", conditionEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存处方");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (registrationEdit->text().trimmed().isEmpty() || drugEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "挂号单号和药品不能为空。");
        return;
    }

    common::Request request;
    request.module = "prescription";
    request.action = "create";
    request.payload["挂号单号"] = registrationEdit->text().trimmed();
    request.payload["药品名称"] = drugEdit->text().trimmed();
    request.payload["数量"] = quantitySpin->value();
    request.payload["用法用量"] = dosageEdit->text().trimmed();
    request.payload["频次"] = frequencyEdit->text().trimmed();
    request.payload["天数"] = daysSpin->value();
    request.payload["过敏史"] = allergyEdit->text().trimmed();
    request.payload["患者情况"] = conditionEdit->text().trimmed();
    apiClient()->send(request);
}

void PrescriptionPage::reviewPrescription()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择处方", "请先在表格中点击要审核的处方。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("处方审核");
    auto* form = new QFormLayout(&dialog);
    auto* actionBox = new QComboBox(&dialog);
    actionBox->addItems({"审核通过", "驳回"});
    auto* reasonEdit = new QLineEdit(&dialog);
    reasonEdit->setPlaceholderText("驳回时必须填写原因，医生可在处方列表查看");
    form->addRow("处方号", new QLineEdit(row.value("处方号").toString(), &dialog));
    form->addRow("患者信息", new QLineEdit(QString("%1 / %2")
        .arg(row.value("患者").toString(), row.value("身份证号").toString()), &dialog));
    form->addRow("药品明细", new QLineEdit(row.value("药品明细").toString(), &dialog));
    form->addRow("审核结果", actionBox);
    form->addRow("驳回原因", reasonEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("提交审核");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (actionBox->currentText() == "驳回" && reasonEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少驳回原因", "驳回处方必须填写驳回原因。");
        return;
    }
    common::Request request;
    request.module = "prescription";
    if (actionBox->currentText() == "驳回") {
        request.action = "reject";
    } else {
        request.action = "review";
    }
    request.payload = row;
    request.payload["驳回原因"] = reasonEdit->text().trimmed();
    apiClient()->send(request);
}

void PrescriptionPage::dispensePrescription()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择处方", "请先在表格中点击要发药的处方。");
        return;
    }
    const QString details = QString("患者信息：%1 / %2\n挂号单号：%3\n药品明细：%4\n处方金额：%5\n当前状态：%6\n\n系统将校验账单是否已缴费；确认发药后会扣减库存。")
        .arg(row.value("患者").toString(),
             row.value("身份证号").toString(),
             row.value("挂号单号").toString(),
             row.value("药品明细").toString(),
             row.value("处方金额").toVariant().toString(),
             row.value("状态").toString());
    if (QMessageBox::question(this, "确认发药", details) != QMessageBox::Yes) {
        return;
    }
    common::Request request;
    request.module = "prescription";
    request.action = "dispense";
    request.payload = row;
    apiClient()->send(request);
}

void PrescriptionPage::returnPrescription()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择处方", "请先在表格中点击要退药的处方。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("退药入库");
    auto* form = new QFormLayout(&dialog);
    auto* reasonEdit = new QLineEdit(&dialog);
    reasonEdit->setPlaceholderText("例如：患者退费申请，药品原包装完好");
    form->addRow("处方号", new QLineEdit(row.value("处方号").toString(), &dialog));
    form->addRow("患者信息", new QLineEdit(QString("%1 / %2")
        .arg(row.value("患者").toString(), row.value("身份证号").toString()), &dialog));
    form->addRow("药品明细", new QLineEdit(row.value("药品明细").toString(), &dialog));
    form->addRow("退药原因", reasonEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("确认退药");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (reasonEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少退药原因", "退药必须填写退药原因。");
        return;
    }

    common::Request request;
    request.module = "prescription";
    request.action = "return";
    request.payload = row;
    request.payload["退药原因"] = reasonEdit->text().trimmed();
    apiClient()->send(request);
}

} // namespace hospital::client
