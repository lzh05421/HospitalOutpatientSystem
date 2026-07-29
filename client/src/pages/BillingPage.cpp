#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace hospital::client {

BillingPage::BillingPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("收费结算", "生成费用清单，完成挂号费、药品费和其他费用支付记录。", "billing", "list", apiClient, parent, 20000)
{
    auto* actions = new QHBoxLayout();
    auto* payButton = new QPushButton("确认收费", this);
    auto* refundButton = new QPushButton("申请退费", this);
    auto* reviewRefundButton = new QPushButton("审核退费", this);
    payButton->setObjectName("primaryButton");
    refundButton->setObjectName("warningButton");
    reviewRefundButton->setObjectName("warningButton");
    actions->addWidget(payButton);
    actions->addWidget(refundButton);
    actions->addWidget(reviewRefundButton);
    actions->addStretch();
    layout()->addItem(actions);
    payButton->setVisible(apiClient && (apiClient->hasPermission("billing:pay")
                                        || apiClient->hasPermission("billing:medicalInsurancePay")));
    refundButton->setVisible(apiClient && apiClient->hasPermission("billing:requestRefund"));
    reviewRefundButton->setVisible(apiClient && apiClient->hasPermission("billing:reviewRefund"));

    connect(payButton, &QPushButton::clicked, this, &BillingPage::paySelectedBill);
    connect(refundButton, &QPushButton::clicked, this, &BillingPage::refundSelectedBill);
    connect(reviewRefundButton, &QPushButton::clicked, this, &BillingPage::reviewSelectedRefund);
}

void BillingPage::paySelectedBill()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择账单", "请先在表格中点击一条待收费账单。");
        return;
    }
    const QString status = row.value("状态").toString();
    if (status == "PAID" || status == "已缴费") {
        QMessageBox::information(this, "无需重复收费", "当前账单已经是已缴费状态。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("确认收费");
    auto* form = new QFormLayout(&dialog);
    auto* methodBox = new QComboBox(&dialog);
    methodBox->addItems({"现金", "微信支付", "支付宝", "银行卡", "医保"});
    auto* invoiceEdit = new QLineEdit("INV" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss"), &dialog);
    form->addRow("账单号", new QLineEdit(row.value("账单号").toString(), &dialog));
    form->addRow("支付方式", methodBox);
    form->addRow("发票号", invoiceEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    common::Request request;
    request.module = "billing";
    request.action = methodBox->currentText() == "医保" ? "medicalInsurancePay" : "pay";
    request.payload = row;
    request.payload["支付方式"] = methodBox->currentText();
    request.payload["发票号"] = invoiceEdit->text().trimmed();
    apiClient()->send(request);
}

void BillingPage::refundSelectedBill()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择账单", "请先在表格中点击一条已缴费账单。");
        return;
    }
    const QString status = row.value("状态").toString();
    if (status != "PAID" && status != "已缴费") {
        QMessageBox::warning(this, "不能退费", "只有已缴费账单可以退费。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("申请退费");
    auto* form = new QFormLayout(&dialog);
    auto* billEdit = new QLineEdit(row.value("账单号").toString(), &dialog);
    billEdit->setReadOnly(true);
    auto* reasonEdit = new QLineEdit(&dialog);
    reasonEdit->setPlaceholderText("请填写退费原因");
    form->addRow("账单号", billEdit);
    form->addRow("退费原因", reasonEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (reasonEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少原因", "退费原因不能为空。");
        return;
    }

    common::Request request;
    request.module = "billing";
    request.action = "requestRefund";
    request.payload = row;
    request.payload["退费原因"] = reasonEdit->text().trimmed();
    apiClient()->send(request);
}

void BillingPage::reviewSelectedRefund()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择账单", "请先在表格中点击一条待审核退费申请。");
        return;
    }
    const QString refundStatus = row.value("退费状态").toString().trimmed();
    if (refundStatus != "待审核" && refundStatus != "PENDING") {
        QMessageBox::warning(this, "不能审核", "只有待审核退费申请可以审核。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("审核退费");
    auto* form = new QFormLayout(&dialog);
    auto* billEdit = new QLineEdit(row.value("账单号").toString(), &dialog);
    billEdit->setReadOnly(true);
    auto* resultBox = new QComboBox(&dialog);
    resultBox->addItems({"通过", "拒绝"});
    auto* noteEdit = new QLineEdit(&dialog);
    noteEdit->setPlaceholderText("审核意见");
    auto* reasonDisplay = new QLineEdit(row.value("退费原因").toString(), &dialog);
    reasonDisplay->setReadOnly(true);
    form->addRow("账单号", billEdit);
    form->addRow("退费原因", reasonDisplay);
    form->addRow("审核结果", resultBox);
    form->addRow("审核意见", noteEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    common::Request request;
    request.module = "billing";
    request.action = "reviewRefund";
    request.payload = row;
    request.payload["审核结果"] = resultBox->currentText();
    request.payload["审核意见"] = noteEdit->text().trimmed();
    apiClient()->send(request);
}

} // namespace hospital::client
