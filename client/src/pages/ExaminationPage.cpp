#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace hospital::client {

ExaminationPage::ExaminationPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("检查检验", "医生开立检查申请，检查完成后回传结果，患者回到候诊队列继续接诊。",
                 "examination", "list", apiClient, parent, 10000)
{
    auto* actions = new QHBoxLayout();
    auto* maintainButton = new QPushButton("维护检查项目", this);
    auto* createButton = new QPushButton("开检查单", this);
    auto* completeButton = new QPushButton("录入检查结果", this);
    maintainButton->setObjectName("secondaryButton");
    createButton->setObjectName("primaryButton");
    completeButton->setObjectName("primaryButton");
    actions->addWidget(maintainButton);
    actions->addWidget(createButton);
    actions->addWidget(completeButton);
    actions->addStretch();

    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        root->insertLayout(3, actions);
    }

    maintainButton->setVisible(apiClient && apiClient->hasPermission("examination:saveItem"));
    createButton->setVisible(apiClient && apiClient->hasPermission("examination:create"));
    completeButton->setVisible(apiClient && apiClient->hasPermission("examination:complete"));

    connect(maintainButton, &QPushButton::clicked, this, &ExaminationPage::saveExaminationItem);
    connect(createButton, &QPushButton::clicked, this, &ExaminationPage::createExamination);
    connect(completeButton, &QPushButton::clicked, this, &ExaminationPage::completeExamination);
    connect(apiClient, &ApiClient::responseReceived, this, &ExaminationPage::onExaminationResponse);
    loadExaminationItems();
}

void ExaminationPage::onExaminationResponse(const common::Response& response)
{
    if (response.data.value("module").toString() != "examination") {
        return;
    }
    if (response.data.value("action").toString() == "items" && response.success) {
        m_examinationItems = response.data.value("rows").toArray();
        return;
    }
    const QString action = response.data.value("action").toString();
    if (response.success && (action == "saveItem" || action == "deleteItem")) {
        loadExaminationItems();
    }
}

void ExaminationPage::loadExaminationItems()
{
    common::Request request;
    request.module = "examination";
    request.action = "items";
    apiClient()->send(request);
}

void ExaminationPage::saveExaminationItem()
{
    QDialog dialog(this);
    dialog.setWindowTitle("维护检查项目");
    auto* form = new QFormLayout(&dialog);
    auto* codeEdit = new QLineEdit(&dialog);
    auto* nameEdit = new QLineEdit(&dialog);
    auto* categoryEdit = new QLineEdit("检查", &dialog);
    auto* priceSpin = new QDoubleSpinBox(&dialog);
    priceSpin->setRange(0, 999999);
    priceSpin->setDecimals(2);
    priceSpin->setPrefix("￥");
    form->addRow("项目编码", codeEdit);
    form->addRow("检查项目", nameEdit);
    form->addRow("项目分类", categoryEdit);
    form->addRow("单价", priceSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存项目");
    buttons->button(QDialogButtonBox::Discard)->setText("停用项目");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(buttons->button(QDialogButtonBox::Discard), &QPushButton::clicked, this, [this, &dialog, codeEdit, nameEdit]() {
        common::Request request;
        request.module = "examination";
        request.action = "deleteItem";
        request.payload["项目编码"] = codeEdit->text().trimmed();
        request.payload["检查项目"] = nameEdit->text().trimmed();
        apiClient()->send(request);
        dialog.reject();
    });
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (codeEdit->text().trimmed().isEmpty() || nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "项目编码和检查项目不能为空。");
        return;
    }

    common::Request request;
    request.module = "examination";
    request.action = "saveItem";
    request.payload["项目编码"] = codeEdit->text().trimmed();
    request.payload["检查项目"] = nameEdit->text().trimmed();
    request.payload["项目分类"] = categoryEdit->text().trimmed();
    request.payload["单价"] = priceSpin->value();
    apiClient()->send(request);
}

void ExaminationPage::deleteExaminationItem()
{
    saveExaminationItem();
}

void ExaminationPage::createExamination()
{
    QDialog dialog(this);
    dialog.setWindowTitle("开检查单");
    auto* form = new QFormLayout(&dialog);
    auto* registrationEdit = new QLineEdit(&dialog);
    auto* itemBox = new QComboBox(&dialog);
    auto* noteEdit = new QTextEdit(&dialog);
    registrationEdit->setPlaceholderText("输入挂号单号");
    for (const auto& item : m_examinationItems) {
        const auto object = item.toObject();
        const QString name = object.value("检查项目").toString().trimmed();
        if (!name.isEmpty() && object.value("状态").toString() != "停用") {
            itemBox->addItem(QString("%1（%2元）").arg(name).arg(object.value("单价").toVariant().toDouble()), name);
        }
    }
    itemBox->setEditable(true);
    itemBox->lineEdit()->setPlaceholderText("选择或输入检查项目");
    noteEdit->setPlaceholderText("填写检查目的或注意事项");
    noteEdit->setMinimumHeight(90);
    form->addRow("挂号单号", registrationEdit);
    form->addRow("检查项目", itemBox);
    form->addRow("申请说明", noteEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存检查单");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString itemName = itemBox->currentData().toString().isEmpty()
        ? itemBox->currentText().trimmed()
        : itemBox->currentData().toString().trimmed();
    if (registrationEdit->text().trimmed().isEmpty() || itemName.isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "挂号单号和检查项目不能为空。");
        return;
    }

    common::Request request;
    request.module = "examination";
    request.action = "create";
    request.payload["挂号单号"] = registrationEdit->text().trimmed();
    request.payload["检查项目"] = itemName;
    request.payload["申请说明"] = noteEdit->toPlainText().trimmed();
    apiClient()->send(request);
}

void ExaminationPage::completeExamination()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择检查单", "请先在表格中点击要录入结果的检查单。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("录入检查结果");
    auto* form = new QFormLayout(&dialog);
    auto* examNoEdit = new QLineEdit(row.value("检查单号").toString(), &dialog);
    examNoEdit->setReadOnly(true);
    auto* findingEdit = new QTextEdit(row.value("报告所见").toString(row.value("检查结果").toString()), &dialog);
    auto* conclusionEdit = new QTextEdit(row.value("报告结论").toString(), &dialog);
    auto* attachmentEdit = new QLineEdit(row.value("报告附件").toString(), &dialog);
    auto* browseButton = new QPushButton("选择附件", &dialog);
    auto* attachmentLayout = new QHBoxLayout();
    attachmentLayout->addWidget(attachmentEdit, 1);
    attachmentLayout->addWidget(browseButton);
    findingEdit->setMinimumHeight(110);
    conclusionEdit->setMinimumHeight(90);
    form->addRow("检查单号", examNoEdit);
    form->addRow("报告所见", findingEdit);
    form->addRow("报告结论", conclusionEdit);
    form->addRow("报告附件", attachmentLayout);
    connect(browseButton, &QPushButton::clicked, &dialog, [attachmentEdit, &dialog]() {
        const QString fileName = QFileDialog::getOpenFileName(
            &dialog,
            "选择报告附件",
            QString(),
            "报告文件 (*.pdf *.png *.jpg *.jpeg *.bmp *.txt *.doc *.docx);;所有文件 (*.*)");
        if (!fileName.isEmpty()) {
            attachmentEdit->setText(fileName);
        }
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存结果");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    common::Request request;
    request.module = "examination";
    request.action = "complete";
    request.payload["检查单号"] = examNoEdit->text().trimmed();
    request.payload["报告所见"] = findingEdit->toPlainText().trimmed();
    request.payload["报告结论"] = conclusionEdit->toPlainText().trimmed();
    request.payload["报告附件"] = attachmentEdit->text().trimmed();
    apiClient()->send(request);
}

} // namespace hospital::client
