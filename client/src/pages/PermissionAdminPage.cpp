#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

QTableWidgetItem* readOnlyItem(const QString& text, const QJsonValue& rawValue = QJsonValue())
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setToolTip(text);
    if (!rawValue.isUndefined()) {
        item->setData(Qt::UserRole, rawValue.toVariant());
    }
    return item;
}

QString statusText(int status)
{
    return status == 0 ? QString("停用") : QString("正常");
}

} // namespace

PermissionAdminPage::PermissionAdminPage(ApiClient* apiClient, QWidget* parent)
    : QWidget(parent)
    , m_apiClient(apiClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* titleBlock = new QVBoxLayout();
    auto* title = new QLabel("权限配置", this);
    title->setObjectName("pageTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* description = new QLabel("维护医护账号、角色归属和细粒度功能权限。", this);
    description->setObjectName("pageDescription");
    titleBlock->addWidget(title);
    titleBlock->addWidget(description);
    auto* refreshButton = new QPushButton("刷新", this);
    refreshButton->setObjectName("primaryButton");
    header->addLayout(titleBlock, 1);
    header->addWidget(refreshButton, 0, Qt::AlignTop);
    root->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* userPanel = new QWidget(splitter);
    auto* userLayout = new QVBoxLayout(userPanel);
    userLayout->setContentsMargins(0, 0, 8, 0);
    userLayout->setSpacing(12);

    auto* userBox = new QGroupBox("用户账号管理", userPanel);
    auto* userBoxLayout = new QVBoxLayout(userBox);
    m_userTable = new QTableWidget(userBox);
    m_userTable->setColumnCount(7);
    m_userTable->setHorizontalHeaderLabels({"用户ID", "账号", "姓名", "电话", "角色编码", "角色名称", "状态"});
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->verticalHeader()->setVisible(false);
    m_userTable->horizontalHeader()->setStretchLastSection(true);
    m_userTable->setAlternatingRowColors(true);
    userBoxLayout->addWidget(m_userTable);

    auto* form = new QFormLayout();
    m_usernameEdit = new QLineEdit(userBox);
    m_usernameEdit->setPlaceholderText("例如：doctor07");
    m_realNameEdit = new QLineEdit(userBox);
    m_realNameEdit->setPlaceholderText("例如：王医生");
    m_phoneEdit = new QLineEdit(userBox);
    m_phoneEdit->setPlaceholderText("手机号或内线");
    m_userRoleBox = new QComboBox(userBox);
    form->addRow("账号", m_usernameEdit);
    form->addRow("姓名", m_realNameEdit);
    form->addRow("电话", m_phoneEdit);
    form->addRow("角色", m_userRoleBox);
    userBoxLayout->addLayout(form);

    auto* userButtons = new QHBoxLayout();
    auto* createButton = new QPushButton("新增账号", userBox);
    createButton->setObjectName("primaryButton");
    auto* resetButton = new QPushButton("重置密码", userBox);
    m_toggleUserButton = new QPushButton("停用账号", userBox);
    userButtons->addWidget(createButton);
    userButtons->addWidget(resetButton);
    userButtons->addWidget(m_toggleUserButton);
    userButtons->addStretch();
    userBoxLayout->addLayout(userButtons);
    userLayout->addWidget(userBox);

    auto* rolePanel = new QWidget(splitter);
    auto* roleLayout = new QVBoxLayout(rolePanel);
    roleLayout->setContentsMargins(8, 0, 0, 0);
    roleLayout->setSpacing(12);

    auto* roleBox = new QGroupBox("角色权限分配", rolePanel);
    auto* roleBoxLayout = new QVBoxLayout(roleBox);
    auto* roleToolbar = new QHBoxLayout();
    auto* roleLabel = new QLabel("角色", roleBox);
    m_roleBox = new QComboBox(roleBox);
    roleToolbar->addWidget(roleLabel);
    roleToolbar->addWidget(m_roleBox, 1);
    roleBoxLayout->addLayout(roleToolbar);

    m_permissionList = new QListWidget(roleBox);
    m_permissionList->setAlternatingRowColors(true);
    m_permissionList->setSelectionMode(QAbstractItemView::NoSelection);
    roleBoxLayout->addWidget(m_permissionList, 1);

    auto* saveButton = new QPushButton("保存角色权限", roleBox);
    saveButton->setObjectName("primaryButton");
    roleBoxLayout->addWidget(saveButton, 0, Qt::AlignRight);
    roleLayout->addWidget(roleBox);

    splitter->addWidget(userPanel);
    splitter->addWidget(rolePanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    setStyleSheet(R"(
        QLabel#pageTitle {
            color: #18212f;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#pageDescription {
            color: #667085;
            font-size: 13px;
        }
        QListWidget {
            background: #ffffff;
            border: 1px solid #d8dee8;
            border-radius: 8px;
            padding: 6px;
        }
        QListWidget::item {
            min-height: 34px;
            padding: 5px 8px;
            border-radius: 6px;
            color: #334155;
        }
        QListWidget::indicator {
            width: 18px;
            height: 18px;
        }
        QListWidget::indicator:unchecked {
            border: 2px solid #94a3b8;
            border-radius: 4px;
            background: #ffffff;
        }
        QListWidget::indicator:checked {
            border: 2px solid #16a34a;
            border-radius: 4px;
            background: #16a34a;
        }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &PermissionAdminPage::refresh);
    connect(createButton, &QPushButton::clicked, this, &PermissionAdminPage::createUser);
    connect(resetButton, &QPushButton::clicked, this, &PermissionAdminPage::resetPassword);
    connect(m_toggleUserButton, &QPushButton::clicked, this, &PermissionAdminPage::toggleUser);
    connect(saveButton, &QPushButton::clicked, this, &PermissionAdminPage::saveRolePermissions);
    connect(m_userTable, &QTableWidget::itemSelectionChanged, this, &PermissionAdminPage::fillUserFormFromSelection);
    connect(m_roleBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PermissionAdminPage::loadSelectedRolePermissions);
    connect(m_permissionList, &QListWidget::itemChanged, this, &PermissionAdminPage::updatePermissionItemVisual);
    connect(m_apiClient, &ApiClient::responseReceived, this, &PermissionAdminPage::onResponseReceived);

    refresh();
}

void PermissionAdminPage::refresh()
{
    requestUsers();
    requestRoles(selectedRoleId());
}

void PermissionAdminPage::requestUsers()
{
    common::Request request;
    request.module = "permissionAdmin";
    request.action = "users";
    m_apiClient->send(request);
}

void PermissionAdminPage::requestRoles(const QString& roleId)
{
    common::Request request;
    request.module = "permissionAdmin";
    request.action = "roles";
    if (!roleId.trimmed().isEmpty()) {
        request.payload["roleId"] = roleId.trimmed();
    }
    m_apiClient->send(request);
}

void PermissionAdminPage::onResponseReceived(const common::Response& response)
{
    if (response.data.value("module").toString() != "permissionAdmin") {
        return;
    }

    if (!response.success) {
        QMessageBox::warning(this, "操作失败", response.message);
        return;
    }

    const QString action = response.data.value("action").toString();
    if (action == "users") {
        renderUsers(response.data.value("rows").toArray());
        return;
    }

    if (action == "roles") {
        renderRoles(response.data.value("roles").toArray(),
                    response.data.value("permissions").toArray(),
                    response.data.value("selectedRoleId").toVariant().toString());
        return;
    }

    const QString message = response.message.trimmed();
    if (!message.isEmpty() && message != "OK") {
        QMessageBox::information(this, "操作成功", message);
        if (action == "saveRolePermissions") {
            QMessageBox::information(this, "重新登录生效", "权限已保存。当前已登录账号需要退出并重新登录后，新权限才会生效。");
        }
    }
    refresh();
}

void PermissionAdminPage::renderUsers(const QJsonArray& rows)
{
    m_userTable->setRowCount(rows.size());
    const QStringList headers = {"用户ID", "账号", "姓名", "电话", "角色编码", "角色名称", "状态"};

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto row = rows.at(rowIndex).toObject();
        for (int column = 0; column < headers.size(); ++column) {
            const QString key = headers.at(column);
            QString text = row.value(key).toVariant().toString();
            if (key == "状态") {
                text = statusText(row.value(key).toVariant().toInt());
            }
            m_userTable->setItem(rowIndex, column, readOnlyItem(text, row.value(key)));
        }
    }
    m_userTable->resizeColumnsToContents();
    m_userTable->horizontalHeader()->setStretchLastSection(true);
}

void PermissionAdminPage::renderRoles(const QJsonArray& roles, const QJsonArray& permissions, const QString& selectedRoleId)
{
    m_roles = roles;

    const QString previousRoleId = selectedRoleId.trimmed().isEmpty() ? this->selectedRoleId() : selectedRoleId.trimmed();
    m_roleBox->blockSignals(true);
    m_userRoleBox->blockSignals(true);
    m_roleBox->clear();
    m_userRoleBox->clear();
    for (const auto& value : roles) {
        const auto role = value.toObject();
        const QString text = QString("%1（%2）")
            .arg(role.value("roleName").toString(), role.value("roleCode").toString());
        const QString roleId = role.value("roleId").toVariant().toString();
        m_roleBox->addItem(text, roleId);
        m_userRoleBox->addItem(text, role.value("roleCode").toString());
    }
    const int roleIndex = m_roleBox->findData(previousRoleId);
    if (roleIndex >= 0) {
        m_roleBox->setCurrentIndex(roleIndex);
    }
    m_roleBox->blockSignals(false);
    m_userRoleBox->blockSignals(false);

    m_permissionList->clear();
    for (const auto& value : permissions) {
        const auto permission = value.toObject();
        const QString code = permission.value("permissionCode").toString();
        const QString label = QString("%1  %2").arg(permission.value("menuName").toString(), code);
        auto* item = new QListWidgetItem(label, m_permissionList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(Qt::UserRole, code);
        item->setData(Qt::UserRole + 1, label);
        item->setData(Qt::CheckStateRole, permission.value("checked").toBool() ? Qt::Checked : Qt::Unchecked);
        updatePermissionItemVisual(item);
    }

    fillUserFormFromSelection();
}

void PermissionAdminPage::updatePermissionItemVisual(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    const bool checked = item->data(Qt::CheckStateRole).toInt() == Qt::Checked;
    const QString baseText = item->data(Qt::UserRole + 1).toString();
    const QString prefix = checked ? "已授权" : "未授权";
    QSignalBlocker blocker(m_permissionList);
    item->setText(QString("[%1] %2").arg(prefix, baseText));
    item->setBackground(checked ? QColor("#dcfce7") : QColor("#f8fafc"));
    item->setForeground(checked ? QColor("#166534") : QColor("#475569"));
    QFont font = item->font();
    font.setBold(checked);
    item->setFont(font);
    item->setToolTip(checked ? "已选中，保存后该角色拥有此权限" : "未选中，保存后该角色没有此权限");
}

QJsonObject PermissionAdminPage::selectedUser() const
{
    const int row = m_userTable->currentRow();
    if (row < 0) {
        return {};
    }

    QJsonObject user;
    for (int column = 0; column < m_userTable->columnCount(); ++column) {
        const auto* header = m_userTable->horizontalHeaderItem(column);
        const auto* item = m_userTable->item(row, column);
        if (!header || !item) {
            continue;
        }
        const QVariant raw = item->data(Qt::UserRole);
        user[header->text()] = QJsonValue::fromVariant(raw.isValid() ? raw : QVariant(item->text()));
    }
    return user;
}

void PermissionAdminPage::fillUserFormFromSelection()
{
    const QJsonObject user = selectedUser();
    if (user.isEmpty()) {
        return;
    }

    m_usernameEdit->setText(user.value("账号").toString());
    m_realNameEdit->setText(user.value("姓名").toString());
    m_phoneEdit->setText(user.value("电话").toString());

    const int roleIndex = m_userRoleBox->findData(user.value("角色编码").toString());
    if (roleIndex >= 0) {
        m_userRoleBox->setCurrentIndex(roleIndex);
    }
    const int status = user.value("状态").toVariant().toInt();
    m_toggleUserButton->setText(status == 0 ? "启用账号" : "停用账号");
}

void PermissionAdminPage::loadSelectedRolePermissions()
{
    requestRoles(selectedRoleId());
}

QString PermissionAdminPage::selectedRoleId() const
{
    return m_roleBox ? m_roleBox->currentData().toString() : QString();
}

QString PermissionAdminPage::selectedRoleCode() const
{
    return m_userRoleBox ? m_userRoleBox->currentData().toString() : QString();
}

void PermissionAdminPage::createUser()
{
    if (m_usernameEdit->text().trimmed().isEmpty() || m_realNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "账号和姓名不能为空。");
        return;
    }

    common::Request request;
    request.module = "permissionAdmin";
    request.action = "createUser";
    request.payload["username"] = m_usernameEdit->text().trimmed();
    request.payload["realName"] = m_realNameEdit->text().trimmed();
    request.payload["phone"] = m_phoneEdit->text().trimmed();
    request.payload["roleCode"] = selectedRoleCode();
    m_apiClient->send(request);
}

void PermissionAdminPage::resetPassword()
{
    const QJsonObject user = selectedUser();
    if (user.isEmpty()) {
        QMessageBox::information(this, "请选择账号", "请先选择要重置密码的账号。");
        return;
    }

    if (QMessageBox::question(this, "确认重置", "确定将该账号密码重置为 123456 吗？") != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = "permissionAdmin";
    request.action = "resetPassword";
    request.payload["userId"] = user.value("用户ID").toVariant().toString();
    request.payload["username"] = user.value("账号").toString();
    m_apiClient->send(request);
}

void PermissionAdminPage::toggleUser()
{
    const QJsonObject user = selectedUser();
    if (user.isEmpty()) {
        QMessageBox::information(this, "请选择账号", "请先选择要启用或停用的账号。");
        return;
    }

    const int currentStatus = user.value("状态").toVariant().toInt();
    const int nextStatus = currentStatus == 0 ? 1 : 0;
    const QString actionText = nextStatus == 0 ? "停用" : "启用";
    if (QMessageBox::question(this, "确认" + actionText, "确定要" + actionText + "该账号吗？") != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = "permissionAdmin";
    request.action = "toggleUser";
    request.payload["userId"] = user.value("用户ID").toVariant().toString();
    request.payload["status"] = nextStatus;
    m_apiClient->send(request);
}

void PermissionAdminPage::saveRolePermissions()
{
    if (selectedRoleId().trimmed().isEmpty()) {
        QMessageBox::information(this, "请选择角色", "请先选择要保存权限的角色。");
        return;
    }

    QJsonArray permissions;
    for (int index = 0; index < m_permissionList->count(); ++index) {
        const auto* item = m_permissionList->item(index);
        if (item && item->data(Qt::CheckStateRole).toInt() == Qt::Checked) {
            permissions.append(item->data(Qt::UserRole).toString());
        }
    }

    common::Request request;
    request.module = "permissionAdmin";
    request.action = "saveRolePermissions";
    request.payload["roleId"] = selectedRoleId();
    request.payload["permissions"] = permissions;
    m_apiClient->send(request);
}

} // namespace hospital::client
