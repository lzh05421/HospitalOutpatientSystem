#include "client/UserCenterWindow.h"

#include "client/ApiClient.h"
#include "client/PatientManager.h"

#include <QHeaderView>
#include <QAbstractItemView>
#include <QCursor>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QTabWidget>

namespace hospital::client {

UserCenterWindow::UserCenterWindow(ApiClient* apiClient, PatientManager* patientManager, QWidget* parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
    , m_patientManager(patientManager)
{
    setWindowTitle("个人中心");
    resize(980, 680);
    setObjectName("userCenterWindow");

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(14);

    auto* header = new QWidget(central);
    header->setObjectName("profileHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 14, 18, 14);
    headerLayout->setSpacing(12);

    auto* title = new QLabel("个人中心", header);
    title->setObjectName("pageTitle");
    m_accountLabel = new QLabel(header);
    m_patientLabel = new QLabel(header);
    auto* switchButton = new QPushButton("切换就诊人", header);
    switchButton->setObjectName("secondaryButton");

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    textLayout->addWidget(title);
    textLayout->addWidget(m_accountLabel);
    textLayout->addWidget(m_patientLabel);

    headerLayout->addLayout(textLayout, 1);
    headerLayout->addWidget(switchButton);

    m_tabs = new QTabWidget(central);
    m_historyTable = new QTableWidget(0, 6, m_tabs);
    m_historyTable->setHorizontalHeaderLabels({"时间", "科室", "医生", "就诊人", "状态", "账单号"});
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->verticalHeader()->setVisible(false);

    m_tabs->addTab(m_historyTable, "挂号记录");
    m_tabs->addTab(createPlaceholderTable("暂无处方单数据"), "处方单");
    m_tabs->addTab(createPlaceholderTable("暂无缴费记录数据"), "缴费记录");

    root->addWidget(header);
    root->addWidget(m_tabs, 1);
    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow#userCenterWindow {
            background: #f3f6fb;
        }
        QWidget#profileHeader {
            background: #ffffff;
            border: 1px solid #d8dee8;
            border-radius: 8px;
        }
        QLabel#pageTitle {
            color: #111827;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel {
            color: #475467;
            font-size: 13px;
        }
        QPushButton {
            min-height: 34px;
            border-radius: 8px;
            border: 1px solid #cbd5e1;
            background: #ffffff;
            color: #1f2937;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton#secondaryButton:hover {
            border-color: #2563eb;
            color: #1d4ed8;
            background: #f8fbff;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #d8dee8;
            border-radius: 8px;
            selection-background-color: #dbeafe;
            selection-color: #111827;
        }
        QHeaderView::section {
            background: #eef4fb;
            color: #334155;
            border: none;
            border-right: 1px solid #d8dee8;
            border-bottom: 1px solid #d8dee8;
            padding: 8px;
            font-weight: 600;
        }
    )");

    connect(switchButton, &QPushButton::clicked, this, &UserCenterWindow::showPatientSwitcher);
    connect(m_patientManager, &PatientManager::currentPatientChanged, this, &UserCenterWindow::refreshHeader);
    connect(m_patientManager, &PatientManager::currentPatientChanged, this, &UserCenterWindow::requestHistory);
    connect(m_patientManager, &PatientManager::patientsLoaded, this, &UserCenterWindow::refreshHeader);
    connect(m_patientManager, &PatientManager::historyLoaded, this, &UserCenterWindow::populateHistory);

    refreshHeader();
    if (m_patientManager->patients().isEmpty()) {
        m_patientManager->loadPatients();
    }
    requestHistory();
}

void UserCenterWindow::refreshHeader()
{
    const QString username = m_apiClient ? m_apiClient->username() : QString();
    m_accountLabel->setText("登录账号：" + (username.isEmpty() ? "未登录" : username));

    const auto patient = m_patientManager->currentPatient();
    const QString tail = patient.idCard.size() >= 4 ? patient.idCard.right(4) : "----";
    m_patientLabel->setText(patient.patientId.isEmpty()
                                ? "当前就诊人：未选择"
                                : QString("当前就诊人：%1（身份证后四位 %2）").arg(patient.name, tail));
}

void UserCenterWindow::showPatientSwitcher()
{
    QMenu menu(this);
    for (const auto& patient : m_patientManager->patients()) {
        const QString tail = patient.idCard.size() >= 4 ? patient.idCard.right(4) : "----";
        auto* action = menu.addAction(QString("%1  %2  后四位%3").arg(patient.name, patient.relationship, tail));
        action->setData(patient.patientId);
    }
    const auto* selected = menu.exec(QCursor::pos());
    if (selected) {
        m_patientManager->selectPatient(selected->data().toString());
    }
}

void UserCenterWindow::populateHistory(const QJsonArray& rows)
{
    m_historyTable->setRowCount(0);
    for (const auto& item : rows) {
        const auto object = item.toObject();
        const int row = m_historyTable->rowCount();
        m_historyTable->insertRow(row);
        const QStringList values = {
            object.value("createdAt").toString(object.value("visitDate").toString()),
            object.value("department").toString(),
            object.value("doctorName").toString(),
            object.value("patientName").toString(),
            object.value("status").toString(),
            object.value("billNo").toString()
        };
        for (int column = 0; column < values.size(); ++column) {
            m_historyTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}

void UserCenterWindow::requestHistory()
{
    if (m_patientManager) {
        m_patientManager->loadMyHistory(true);
    }
}

QTableWidget* UserCenterWindow::createPlaceholderTable(const QString& text)
{
    auto* table = new QTableWidget(1, 1, this);
    table->setHorizontalHeaderLabels({text});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setItem(0, 0, new QTableWidgetItem(text));
    return table;
}

} // namespace hospital::client
