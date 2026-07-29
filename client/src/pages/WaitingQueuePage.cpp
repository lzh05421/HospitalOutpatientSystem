#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

QFrame* createMetricCard(const QString& title, QLabel** valueLabel, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("queueMetricCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("queueMetricTitle");
    auto* value = new QLabel("--", card);
    value->setObjectName("queueMetricValue");
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

} // namespace

WaitingQueuePage::WaitingQueuePage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("候诊队列",
                 "挂号成功后进入候诊队列；叫号后转入医生接诊，检查完成后回到这里等待复诊。",
                 "registration",
                 "waiting",
                 apiClient,
                 parent,
                 5000)
{
    auto* summaryLayout = new QHBoxLayout();
    summaryLayout->setSpacing(12);
    summaryLayout->addWidget(createMetricCard("待叫号", &m_waitingCountLabel, this));
    summaryLayout->addWidget(createMetricCard("已叫号", &m_calledCountLabel, this));
    summaryLayout->addWidget(createMetricCard("急诊优先", &m_emergencyCountLabel, this));
    summaryLayout->addWidget(createMetricCard("平均等待", &m_averageWaitLabel, this));

    auto* flowBar = new QFrame(this);
    flowBar->setObjectName("queueFlowBar");
    auto* flowLayout = new QHBoxLayout(flowBar);
    flowLayout->setContentsMargins(14, 10, 14, 10);
    flowLayout->setSpacing(10);
    flowLayout->addWidget(createStatusChip("1 待叫号", "queueChipWaiting", flowBar));
    flowLayout->addWidget(createStatusChip("2 已叫号", "queueChipCalled", flowBar));
    flowLayout->addWidget(createStatusChip("3 检查完成待复诊", "queueChipReview", flowBar));
    flowLayout->addStretch();
    auto* flowHint = new QLabel("当前页优先展示当天排队患者，急诊优先会自动排到前面。", flowBar);
    flowHint->setObjectName("queueFlowHint");
    flowLayout->addWidget(flowHint);

    auto* actions = new QHBoxLayout();
    auto* callButton = new QPushButton("叫号", this);
    auto* emergencyButton = new QPushButton("设为急诊优先", this);
    callButton->setObjectName("primaryButton");
    emergencyButton->setObjectName("warningButton");
    actions->addWidget(callButton);
    actions->addWidget(emergencyButton);
    actions->addStretch();
    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        root->insertLayout(3, summaryLayout);
        root->insertWidget(4, flowBar);
        root->insertLayout(5, actions);
    }
    setStyleSheet(styleSheet() + R"(
        QFrame#queueMetricCard {
            background: #ffffff;
            border: 1px solid #d9e6e2;
            border-radius: 10px;
        }
        QLabel#queueMetricTitle {
            color: #5f7080;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#queueMetricValue {
            color: #0f172a;
            font-size: 28px;
            font-weight: 700;
        }
        QFrame#queueFlowBar {
            background: #f7fbfb;
            border: 1px solid #d9e6e2;
            border-radius: 10px;
        }
        QLabel#queueChipWaiting, QLabel#queueChipCalled, QLabel#queueChipReview {
            padding: 6px 12px;
            border-radius: 999px;
            font-weight: 600;
        }
        QLabel#queueChipWaiting {
            color: #9a3412;
            background: #fff7ed;
            border: 1px solid #fed7aa;
        }
        QLabel#queueChipCalled {
            color: #0f766e;
            background: #f0fdfa;
            border: 1px solid #99f6e4;
        }
        QLabel#queueChipReview {
            color: #1d4ed8;
            background: #eff6ff;
            border: 1px solid #bfdbfe;
        }
        QLabel#queueFlowHint {
            color: #5f7080;
        }
    )");
    connect(callButton, &QPushButton::clicked, this, &WaitingQueuePage::callSelectedPatient);
    connect(emergencyButton, &QPushButton::clicked, this, &WaitingQueuePage::markSelectedEmergency);
}

void WaitingQueuePage::rowsUpdated(const QJsonArray& rows)
{
    int waitingCount = 0;
    int calledCount = 0;
    int emergencyCount = 0;
    int waitMinutesTotal = 0;
    int waitMinutesCount = 0;

    for (const auto& item : rows) {
        const QJsonObject row = item.toObject();
        const QString status = row.value("候诊状态").toString();
        if (status == "待叫号") {
            ++waitingCount;
        } else if (status == "已叫号") {
            ++calledCount;
        } else if (status == "检查完成待复诊") {
            ++calledCount;
        }
        if (row.value("急诊标识").toString().contains("急诊优先")) {
            ++emergencyCount;
        }

        QString waitText = row.value("预计等待").toString().trimmed();
        waitText.remove("分钟");
        bool ok = false;
        const int minutes = waitText.toInt(&ok);
        if (ok) {
            waitMinutesTotal += minutes;
            ++waitMinutesCount;
        }
    }

    if (m_waitingCountLabel) {
        m_waitingCountLabel->setText(QString::number(waitingCount));
    }
    if (m_calledCountLabel) {
        m_calledCountLabel->setText(QString::number(calledCount));
    }
    if (m_emergencyCountLabel) {
        m_emergencyCountLabel->setText(QString::number(emergencyCount));
    }
    if (m_averageWaitLabel) {
        const int average = waitMinutesCount == 0 ? 0 : waitMinutesTotal / waitMinutesCount;
        m_averageWaitLabel->setText(QString("%1 分钟").arg(average));
    }
}

void WaitingQueuePage::callSelectedPatient()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择患者", "请先在候诊队列中点击要叫号的患者。");
        return;
    }

    common::Request request;
    request.module = "registration";
    request.action = "call";
    request.payload = row;
    apiClient()->send(request);
}

void WaitingQueuePage::markSelectedEmergency()
{
    QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择患者", "请先在候诊队列中点击要设置急诊优先的患者。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("设置急诊优先");
    auto* form = new QFormLayout(&dialog);
    auto* registrationEdit = new QLineEdit(row.value("挂号单号").toString(), &dialog);
    registrationEdit->setReadOnly(true);
    auto* patientEdit = new QLineEdit(row.value("患者").toString(), &dialog);
    patientEdit->setReadOnly(true);
    auto* reasonEdit = new QLineEdit(&dialog);
    reasonEdit->setPlaceholderText("例如：胸痛、高热、外伤、呼吸困难");
    form->addRow("挂号单号", registrationEdit);
    form->addRow("患者", patientEdit);
    form->addRow("急诊原因", reasonEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("确认设置");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString reason = reasonEdit->text().trimmed();
    if (reason.isEmpty()) {
        QMessageBox::warning(this, "急诊原因不能为空", "请填写急诊原因后再设置急诊优先。");
        return;
    }

    row["急诊原因"] = reason;
    common::Request request;
    request.module = "registration";
    request.action = "markEmergency";
    request.payload = row;
    apiClient()->send(request);
}

} // namespace hospital::client
