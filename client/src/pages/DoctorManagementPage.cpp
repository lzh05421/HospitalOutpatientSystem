#include "client/pages/Pages.h"

#include "client/ApiClient.h"
#include "client/DepartmentCatalog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace hospital::client {

DoctorManagementPage::DoctorManagementPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("医生管理", "维护医生基本信息、科室、职称、擅长方向和挂号费。", "doctor", "list", apiClient, parent, 0)
{
    auto* box = new QGroupBox("医生信息维护", this);
    auto* form = new QGridLayout(box);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    m_nameEdit = new QLineEdit(box);
    m_categoryBox = new QComboBox(box);
    m_categoryBox->setEditable(true);
    m_categoryBox->setInsertPolicy(QComboBox::NoInsert);
    m_categoryBox->addItems(DepartmentCatalog::categories());

    m_departmentBox = new QComboBox(box);
    m_departmentBox->setEditable(true);
    m_departmentBox->setInsertPolicy(QComboBox::NoInsert);
    m_titleEdit = new QLineEdit(box);
    m_specialtyEdit = new QLineEdit(box);
    m_phoneEdit = new QLineEdit(box);
    m_feeSpin = new QSpinBox(box);
    m_feeSpin->setRange(0, 200);
    m_feeSpin->setSuffix(" 元");

    auto* addButton = new QPushButton("新增医生", box);
    auto* updateButton = new QPushButton("修改选中医生", box);
    auto* deleteButton = new QPushButton("删除选中医生", box);
    addButton->setVisible(apiClient->hasPermission("doctor:create"));
    updateButton->setVisible(apiClient->hasPermission("doctor:update"));
    deleteButton->setVisible(apiClient->hasPermission("doctor:delete"));

    form->addWidget(new QLabel("医生姓名", box), 0, 0);
    form->addWidget(m_nameEdit, 0, 1);
    form->addWidget(new QLabel("门诊大类", box), 0, 2);
    form->addWidget(m_categoryBox, 0, 3);
    form->addWidget(new QLabel("所属科室", box), 0, 4);
    form->addWidget(m_departmentBox, 0, 5);

    form->addWidget(new QLabel("职称", box), 1, 0);
    form->addWidget(m_titleEdit, 1, 1);
    form->addWidget(new QLabel("擅长方向", box), 1, 2);
    form->addWidget(m_specialtyEdit, 1, 3);
    form->addWidget(new QLabel("电话", box), 1, 4);
    form->addWidget(m_phoneEdit, 1, 5);

    form->addWidget(new QLabel("挂号费", box), 2, 0);
    form->addWidget(m_feeSpin, 2, 1);
    form->addWidget(addButton, 2, 3);
    form->addWidget(updateButton, 2, 4);
    form->addWidget(deleteButton, 2, 5);
    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);
    form->setColumnStretch(5, 1);

    layout()->addWidget(box);

    connect(m_categoryBox, &QComboBox::currentTextChanged, this, &DoctorManagementPage::updateSpecialtyOptions);
    connect(addButton, &QPushButton::clicked, this, &DoctorManagementPage::addDoctor);
    connect(updateButton, &QPushButton::clicked, this, &DoctorManagementPage::updateSelectedDoctor);
    connect(deleteButton, &QPushButton::clicked, this, &DoctorManagementPage::deleteSelectedDoctor);
    connect(tableWidget(), &QTableWidget::itemSelectionChanged, this, &DoctorManagementPage::fillFormFromSelection);
    updateSpecialtyOptions();
}

void DoctorManagementPage::rowsUpdated(const QJsonArray& rows)
{
    QString current = m_departmentBox ? m_departmentBox->currentText().trimmed() : QString();
    for (const auto& item : rows) {
        const QString department = item.toObject().value("所属科室").toString().trimmed();
        const QString category = DepartmentCatalog::categoryFor(department);
        if (!category.isEmpty() && m_categoryBox && m_categoryBox->findText(category) < 0) {
            m_categoryBox->addItem(category);
        }
    }

    updateSpecialtyOptions();
    if (!current.isEmpty()) {
        const QString category = DepartmentCatalog::categoryFor(current);
        const QString specialty = DepartmentCatalog::specialtyFor(current);
        if (!category.isEmpty() && m_categoryBox) {
            m_categoryBox->setCurrentText(category);
            updateSpecialtyOptions();
        }
        const int index = m_departmentBox->findText(specialty.isEmpty() ? current : specialty);
        if (index >= 0) {
            m_departmentBox->setCurrentIndex(index);
        }
    }
}

void DoctorManagementPage::updateSpecialtyOptions()
{
    if (!m_categoryBox || !m_departmentBox) {
        return;
    }

    const QString current = m_departmentBox->currentText().trimmed();
    const QString currentSpecialty = DepartmentCatalog::specialtyFor(current);
    QStringList departments = DepartmentCatalog::specialties(m_categoryBox->currentText());

    m_departmentBox->blockSignals(true);
    m_departmentBox->clear();
    m_departmentBox->addItems(departments);
    if (!current.isEmpty()) {
        const int index = m_departmentBox->findText(currentSpecialty.isEmpty() ? current : currentSpecialty);
        if (index >= 0) {
            m_departmentBox->setCurrentIndex(index);
        } else {
            m_departmentBox->setEditText(current);
        }
    }
    m_departmentBox->blockSignals(false);
}

void DoctorManagementPage::addDoctor()
{
    if (!apiClient()->hasPermission("doctor:create")) {
        QMessageBox::warning(this, "权限不足", "当前账号没有新增医生权限。");
        return;
    }

    const QString name = m_nameEdit->text().trimmed();
    const QString department = m_departmentBox->currentText().trimmed();
    if (name.isEmpty() || department.isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "医生姓名和所属科室不能为空。");
        return;
    }

    common::Request request;
    request.module = "doctor";
    request.action = "create";
    request.payload["医生姓名"] = name;
    request.payload["所属科室"] = department;
    request.payload["职称"] = m_titleEdit->text().trimmed();
    request.payload["擅长方向"] = m_specialtyEdit->text().trimmed();
    request.payload["电话"] = m_phoneEdit->text().trimmed();
    request.payload["挂号费"] = m_feeSpin->value();
    apiClient()->send(request);
}

void DoctorManagementPage::fillFormFromSelection()
{
    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        return;
    }

    m_nameEdit->setText(row.value("医生姓名").toString());
    const QString department = row.value("所属科室").toString();
    const QString category = DepartmentCatalog::categoryFor(department);
    const QString specialty = DepartmentCatalog::specialtyFor(department);
    if (!category.isEmpty()) {
        m_categoryBox->setCurrentText(category);
        updateSpecialtyOptions();
    }
    m_departmentBox->setCurrentText(specialty.isEmpty() ? department : specialty);
    m_titleEdit->setText(row.value("职称").toString());
    m_specialtyEdit->setText(row.value("擅长方向").toString());
    m_phoneEdit->setText(row.value("电话").toString());
    m_feeSpin->setValue(row.value("挂号费").toVariant().toInt());
}

void DoctorManagementPage::updateSelectedDoctor()
{
    if (!apiClient()->hasPermission("doctor:update")) {
        QMessageBox::warning(this, "权限不足", "当前账号没有修改医生权限。");
        return;
    }

    QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择医生", "请先在表格中点击要修改的医生。");
        return;
    }

    row["原医生姓名"] = row.value("医生姓名").toString();
    row["医生姓名"] = m_nameEdit->text().trimmed();
    row["所属科室"] = m_departmentBox->currentText().trimmed();
    row["职称"] = m_titleEdit->text().trimmed();
    row["擅长方向"] = m_specialtyEdit->text().trimmed();
    row["电话"] = m_phoneEdit->text().trimmed();
    row["挂号费"] = m_feeSpin->value();
    row["状态"] = 1;

    common::Request request;
    request.module = "doctor";
    request.action = "update";
    request.payload = row;
    apiClient()->send(request);
}

void DoctorManagementPage::deleteSelectedDoctor()
{
    if (!apiClient()->hasPermission("doctor:delete")) {
        QMessageBox::warning(this, "权限不足", "当前账号没有停用医生权限。");
        return;
    }

    const QJsonObject row = selectedRowObject();
    if (row.isEmpty()) {
        QMessageBox::information(this, "请选择医生", "请先在表格中点击要删除/停用的医生。");
        return;
    }

    if (QMessageBox::question(this, "确认删除", "确定要停用该医生吗？停用后医生列表和患者预约号源中都不会再显示。") != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = "doctor";
    request.action = "delete";
    request.payload = row;
    apiClient()->send(request);
}

} // namespace hospital::client
