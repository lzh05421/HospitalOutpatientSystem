#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace hospital::client {

DepartmentPage::DepartmentPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("科室管理", "维护医院科室基础信息，医生管理和排班将从这里关联科室。", "department", "list", apiClient, parent)
{
    auto* box = new QGroupBox("科室信息维护", this);
    auto* form = new QFormLayout(box);

    m_codeEdit = new QLineEdit(box);
    m_codeEdit->setPlaceholderText("例如：DEP005，不填则自动生成");
    m_nameEdit = new QLineEdit(box);
    m_nameEdit->setPlaceholderText("例如：眼科");
    m_locationEdit = new QLineEdit(box);
    m_locationEdit->setPlaceholderText("例如：门诊楼五层");

    auto* addButton = new QPushButton("新增科室", box);
    auto* updateButton = new QPushButton("修改选中科室", box);
    auto* deleteButton = new QPushButton("停用选中科室", box);

    form->addRow("科室编码", m_codeEdit);
    form->addRow("科室名称", m_nameEdit);
    form->addRow("位置", m_locationEdit);
    form->addRow(addButton, updateButton);
    form->addRow(deleteButton);

    layout()->addWidget(box);

    connect(addButton, &QPushButton::clicked, this, &DepartmentPage::addDepartment);
    connect(updateButton, &QPushButton::clicked, this, &DepartmentPage::updateSelectedDepartment);
    connect(deleteButton, &QPushButton::clicked, this, &DepartmentPage::deleteSelectedDepartment);
    connect(tableWidget(), &QTableWidget::itemSelectionChanged, this, &DepartmentPage::fillFormFromSelection);
}

void DepartmentPage::rowsUpdated(const QJsonArray&)
{
}

void DepartmentPage::addDepartment()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "科室名称不能为空。");
        return;
    }

    common::Request request;
    request.module = "department";
    request.action = "create";
    request.payload["科室编码"] = m_codeEdit->text().trimmed();
    request.payload["科室名称"] = m_nameEdit->text().trimmed();
    request.payload["位置"] = m_locationEdit->text().trimmed();
    apiClient()->send(request);
}

void DepartmentPage::fillFormFromSelection()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        return;
    }

    m_codeEdit->setText(row.value("科室编码").toString());
    m_nameEdit->setText(row.value("科室名称").toString());
    m_locationEdit->setText(row.value("位置").toString());
}

void DepartmentPage::updateSelectedDepartment()
{
    QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择科室", "请先在表格中点击要修改的科室。");
        return;
    }

    row["原科室编码"] = row.value("科室编码").toString();
    row["科室编码"] = m_codeEdit->text().trimmed();
    row["科室名称"] = m_nameEdit->text().trimmed();
    row["位置"] = m_locationEdit->text().trimmed();
    row["状态"] = 1;

    common::Request request;
    request.module = "department";
    request.action = "update";
    request.payload = row;
    apiClient()->send(request);
}

void DepartmentPage::deleteSelectedDepartment()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择科室", "请先在表格中点击要停用的科室。");
        return;
    }

    if (QMessageBox::question(this, "确认停用", "确定要停用该科室吗？已有医生的科室会保留历史关联，列表默认不再显示。") != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = "department";
    request.action = "delete";
    request.payload = row;
    apiClient()->send(request);
}

} // namespace hospital::client
