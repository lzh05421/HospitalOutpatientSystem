#include "client/MainWindow.h"

#include "client/ApiClient.h"
#include "client/pages/Pages.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <functional>

namespace hospital::client {

MainWindow::MainWindow(ApiClient* apiClient, QWidget* parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
{
    setWindowTitle("医院门诊挂号与药品管理系统");
    resize(1440, 860);
    setMinimumSize(1120, 680);
    setObjectName("mainWindow");

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* sidebar = new QFrame(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(248);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(16, 18, 16, 18);
    sidebarLayout->setSpacing(12);

    auto* sidebarBrandPanel = new QFrame(sidebar);
    sidebarBrandPanel->setObjectName("sidebarBrandPanel");
    auto* brandLayout = new QVBoxLayout(sidebarBrandPanel);
    brandLayout->setContentsMargins(14, 14, 14, 14);
    brandLayout->setSpacing(6);

    auto* moduleStatusPill = new QLabel("在线工作台", sidebarBrandPanel);
    moduleStatusPill->setObjectName("moduleStatusPill");
    moduleStatusPill->setAlignment(Qt::AlignCenter);
    auto* productTitle = new QLabel("门诊业务系统", sidebarBrandPanel);
    productTitle->setObjectName("productTitle");
    auto* productSubTitle = new QLabel("OPD Workstation", sidebarBrandPanel);
    productSubTitle->setObjectName("productSubTitle");
    auto* accountLabel = new QLabel(QString("%1 · %2")
        .arg(m_apiClient->realName().isEmpty() ? m_apiClient->username() : m_apiClient->realName(),
             m_apiClient->roleName().isEmpty() ? m_apiClient->roleCode() : m_apiClient->roleName()), sidebarBrandPanel);
    accountLabel->setObjectName("accountLabel");
    brandLayout->addWidget(moduleStatusPill, 0, Qt::AlignLeft);
    brandLayout->addWidget(productTitle);
    brandLayout->addWidget(productSubTitle);
    brandLayout->addWidget(accountLabel);

    m_navSearch = new QLineEdit(sidebar);
    m_navSearch->setObjectName("navSearch");
    m_navSearch->setPlaceholderText("搜索菜单");
    m_navSearch->setClearButtonEnabled(true);

    m_navigation = new QListWidget(sidebar);
    m_navigation->setObjectName("sideNav");
    m_navigation->setUniformItemSizes(true);
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setSpacing(4);

    auto* navSectionLabel = new QLabel("功能模块", sidebar);
    navSectionLabel->setObjectName("navSectionLabel");

    sidebarLayout->addWidget(sidebarBrandPanel);
    sidebarLayout->addSpacing(6);
    sidebarLayout->addWidget(m_navSearch);
    sidebarLayout->addWidget(navSectionLabel);
    sidebarLayout->addWidget(m_navigation, 1);

    m_pages = new QStackedWidget(central);
    m_pages->setObjectName("contentStack");

    layout->addWidget(sidebar);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);

    const auto addIfAllowed = [this](const QString& title, const QStringList& roles, const std::function<QWidget*()>& factory) {
        if (canAccess(roles)) {
            addModulePage(title, factory(), roles);
        }
    };

    addIfAllowed("院长驾驶舱", {"ADMIN", "DIRECTOR", "CASHIER"}, [this]() { return new DashboardPage(m_apiClient, this); });
    addIfAllowed("患者管理", {"ADMIN", "DIRECTOR", "REGISTRAR", "DOCTOR"}, [this]() { return new PatientPage(m_apiClient, this); });
    addIfAllowed("挂号管理", {"ADMIN", "REGISTRAR"}, [this]() { return new RegistrationPage(m_apiClient, this); });
    addIfAllowed("候诊队列", {"ADMIN", "REGISTRAR", "DOCTOR"}, [this]() { return new WaitingQueuePage(m_apiClient, this); });
    addIfAllowed("患者病历档案", {"ADMIN", "DIRECTOR", "DOCTOR"}, [this]() { return new PatientRecordPage(m_apiClient, this); });
    addIfAllowed("科室管理", {"ADMIN", "DIRECTOR", "REGISTRAR"}, [this]() { return new DepartmentPage(m_apiClient, this); });
    addIfAllowed("医生排班", {"ADMIN", "DIRECTOR", "REGISTRAR"}, [this]() { return new SchedulePage(m_apiClient, this); });
    addIfAllowed("医生管理", {"ADMIN", "DIRECTOR"}, [this]() { return new DoctorManagementPage(m_apiClient, this); });
    addIfAllowed("医生接诊", {"ADMIN", "DIRECTOR", "DOCTOR"}, [this]() { return new ConsultationPage(m_apiClient, this); });
    addIfAllowed("检查检验", {"ADMIN", "DIRECTOR", "DOCTOR"}, [this]() { return new ExaminationPage(m_apiClient, this); });
    addIfAllowed("处方管理", {"ADMIN", "DIRECTOR", "DOCTOR", "PHARMACIST"}, [this]() { return new PrescriptionPage(m_apiClient, this); });
    addIfAllowed("药品库存", {"ADMIN", "PHARMACIST"}, [this]() { return new DrugInventoryPage(m_apiClient, this); });
    addIfAllowed("收费结算", {"ADMIN", "REGISTRAR", "CASHIER"}, [this]() { return new BillingPage(m_apiClient, this); });
    addIfAllowed("费用统计", {"ADMIN", "DIRECTOR", "CASHIER"}, [this]() { return new StatisticsPage(m_apiClient, this); });
    addIfAllowed("操作日志", {"ADMIN"}, [this]() { return new OperationLogPage(m_apiClient, this); });
    addIfAllowed("权限配置", {"ADMIN"}, [this]() { return new PermissionAdminPage(m_apiClient, this); });

    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_navSearch, &QLineEdit::textChanged, this, &MainWindow::filterNavigation);
    if (m_navigation->count() > 0) {
        m_navigation->setCurrentRow(0);
    }

    setStyleSheet(R"(
        QMainWindow#mainWindow {
            background: #eef3f8;
        }
        QFrame#sidebar {
            background: #0f172a;
            border: none;
        }
        QFrame#sidebarBrandPanel {
            background: #172033;
            border: 1px solid #263349;
            border-radius: 14px;
        }
        QLabel#moduleStatusPill {
            color: #67e8f9;
            background: #0f2f3b;
            border: 1px solid #155e75;
            border-radius: 999px;
            padding: 5px 10px;
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#productTitle {
            color: #ffffff;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#productSubTitle {
            color: #9ca3af;
            font-size: 12px;
            letter-spacing: 0px;
        }
        QLabel#accountLabel {
            color: #e5edf5;
            background: #0f172a;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 9px 10px;
            font-size: 13px;
        }
        QLabel#navSectionLabel {
            color: #8fb4bd;
            font-size: 12px;
            font-weight: 700;
            padding: 2px 4px;
        }
        QLineEdit#navSearch {
            min-height: 34px;
            border: 1px solid #334155;
            border-radius: 8px;
            background: #0b1324;
            color: #f9fafb;
            padding: 6px 10px;
        }
        QLineEdit#navSearch:focus {
            border-color: #60a5fa;
        }
        QListWidget#sideNav {
            background: transparent;
            color: #d1d5db;
            border: none;
            font-size: 14px;
            outline: none;
        }
        QListWidget#sideNav::item {
            min-height: 38px;
            padding: 0 12px;
            border-radius: 8px;
            border-left: 3px solid transparent;
        }
        QListWidget#sideNav::item:selected {
            background: #1d4ed8;
            color: white;
            border-left: 3px solid #67e8f9;
        }
        QListWidget#sideNav::item:hover {
            background: #1e293b;
            color: white;
        }
        QStackedWidget#contentStack {
            background: #eef3f8;
        }
        QLabel {
            color: #18212f;
        }
        QPushButton {
            min-height: 34px;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            background: #ffffff;
            color: #1f2937;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            border-color: #0f766e;
            color: #0f766e;
            background: #f0fdfa;
        }
        QPushButton:pressed {
            background: #ccfbf1;
        }
        QPushButton:disabled {
            color: #94a3b8;
            background: #f1f5f9;
            border-color: #dbe3ec;
        }
        QPushButton#primaryButton {
            background: #0f766e;
            border-color: #0f766e;
            color: #ffffff;
        }
        QPushButton#primaryButton:hover {
            background: #0d9488;
            color: #ffffff;
        }
        QPushButton#secondaryButton {
            background: #ffffff;
            color: #334155;
        }
        QPushButton#warningButton {
            background: #fff7ed;
            border-color: #fed7aa;
            color: #c2410c;
        }
        QPushButton#warningButton:hover {
            background: #ffedd5;
            border-color: #fb923c;
            color: #9a3412;
        }
        QPushButton#dangerButton {
            background: #fff1f2;
            border-color: #fecdd3;
            color: #be123c;
        }
        QPushButton#dangerButton:hover {
            background: #ffe4e6;
            border-color: #fb7185;
            color: #9f1239;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f7fafc;
            border: 1px solid #d5dee8;
            border-radius: 8px;
            gridline-color: #edf2f7;
            selection-background-color: #ccfbf1;
            selection-color: #111827;
        }
        QTableWidget::item {
            padding: 4px 8px;
            border-bottom: 1px solid #eef2f7;
        }
        QTableWidget::item:hover {
            background: #f0fdfa;
        }
        QTableWidget::item:selected {
            background: #ccfbf1;
            color: #0f172a;
        }
        QHeaderView::section {
            background: #e8f1f6;
            color: #243447;
            border: none;
            border-right: 1px solid #d5dee8;
            border-bottom: 1px solid #d5dee8;
            padding: 8px;
            font-weight: 600;
        }
        QHeaderView::section:hover {
            background: #dff7f3;
            color: #0f766e;
        }
        QGroupBox {
            border: 1px solid #d8dee8;
            border-radius: 8px;
            margin-top: 14px;
            padding: 14px;
            background: #ffffff;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #1d4ed8;
            font-weight: 600;
        }
        QLineEdit, QComboBox, QDateEdit, QSpinBox, QDoubleSpinBox, QTextEdit {
            min-height: 32px;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            padding: 5px 9px;
            background: #ffffff;
            color: #111827;
        }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus {
            border-color: #0f766e;
            background: #fbfefd;
        }
        QComboBox::drop-down {
            width: 24px;
            border: none;
            border-left: 1px solid #e2e8f0;
            background: #f8fafc;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #eef2f7;
            border: none;
            margin: 0;
        }
        QScrollBar:vertical {
            width: 10px;
        }
        QScrollBar:horizontal {
            height: 10px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #cbd5e1;
            border-radius: 5px;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0;
            height: 0;
        }
    )");
}

bool MainWindow::canAccess(const QStringList& roles) const
{
    if (roles.isEmpty()) {
        return true;
    }
    return roles.contains(m_apiClient->roleCode());
}

void MainWindow::addModulePage(const QString& title, QWidget* page, const QStringList& roles)
{
    if (!canAccess(roles)) {
        page->deleteLater();
        return;
    }
    auto* item = new QListWidgetItem(title, m_navigation);
    item->setToolTip(title);
    m_pages->addWidget(page);
}

void MainWindow::filterNavigation(const QString& keyword)
{
    const QString normalized = keyword.trimmed();
    int firstVisible = -1;

    for (int row = 0; row < m_navigation->count(); ++row) {
        auto* item = m_navigation->item(row);
        const bool matched = normalized.isEmpty()
            || item->text().contains(normalized, Qt::CaseInsensitive);
        item->setHidden(!matched);
        if (matched && firstVisible < 0) {
            firstVisible = row;
        }
    }

    if (firstVisible >= 0 && m_navigation->currentItem() && m_navigation->currentItem()->isHidden()) {
        m_navigation->setCurrentRow(firstVisible);
    }
}

} // namespace hospital::client
