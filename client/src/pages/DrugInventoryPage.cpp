#include "client/pages/Pages.h"

#include "client/ApiClient.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

QStringList defaultCategories()
{
    return {"抗生素", "解热镇痛", "消化系统", "心血管", "中成药", "外用药"};
}

QStringList loadStoredCategories()
{
    QSettings settings("HospitalOutpatientSystem", "Client");
    QStringList categories = settings.value("inventory/categories", defaultCategories()).toStringList();
    for (const QString& category : defaultCategories()) {
        if (!categories.contains(category)) {
            categories.append(category);
        }
    }
    categories.removeDuplicates();
    categories.sort(Qt::CaseInsensitive);
    return categories;
}

void saveCategoriesFromBox(QComboBox* box)
{
    QStringList categories;
    for (int i = 0; i < box->count(); ++i) {
        const QString category = box->itemText(i).trimmed();
        if (!category.isEmpty() && !categories.contains(category)) {
            categories.append(category);
        }
    }
    categories.sort(Qt::CaseInsensitive);
    QSettings settings("HospitalOutpatientSystem", "Client");
    settings.setValue("inventory/categories", categories);
}

void rememberCategory(QComboBox* box, const QString& value)
{
    const QString category = value.trimmed();
    if (category.isEmpty()) {
        return;
    }
    if (box->findText(category, Qt::MatchFixedString) < 0) {
        box->addItem(category);
    }
    saveCategoriesFromBox(box);
}

} // namespace

DrugInventoryPage::DrugInventoryPage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("药品库存", "维护药品基础信息、入库出库记录、库存数量和低库存预警。", "inventory", "list", apiClient, parent)
{
    auto* box = new QGroupBox("药品入库/库存调整", this);
    auto* form = new QFormLayout(box);

    auto* barcodeEdit = new QLineEdit(box);
    barcodeEdit->setPlaceholderText("扫码枪输入后回车，或手动输入条形码");

    auto* drugEdit = new QLineEdit(box);
    drugEdit->setPlaceholderText("可输入库里没有的新药品名称");

    m_categoryBox = new QComboBox(box);
    m_categoryBox->setEditable(true);
    m_categoryBox->setInsertPolicy(QComboBox::NoInsert);
    m_categoryBox->addItems(loadStoredCategories());
    m_categoryBox->lineEdit()->setPlaceholderText("可选择，也可输入新分类");

    auto* specificationEdit = new QLineEdit(box);
    specificationEdit->setPlaceholderText("例如：0.25g*24粒");

    auto* unitBox = new QComboBox(box);
    unitBox->setEditable(true);
    unitBox->addItems({"盒", "瓶", "支", "袋", "片"});

    auto* priceSpin = new QDoubleSpinBox(box);
    priceSpin->setRange(0, 99999);
    priceSpin->setDecimals(2);
    priceSpin->setSuffix(" 元");
    priceSpin->setValue(12.00);

    auto* quantitySpin = new QSpinBox(box);
    quantitySpin->setRange(1, 10000);
    quantitySpin->setValue(50);

    auto* warningSpin = new QSpinBox(box);
    warningSpin->setRange(0, 10000);
    warningSpin->setValue(20);

    auto* expiryEdit = new QDateEdit(QDate::currentDate().addYears(1), box);
    expiryEdit->setCalendarPopup(true);
    expiryEdit->setDisplayFormat("yyyy-MM-dd");

    auto* inButton = new QPushButton("确认入库", box);
    auto* warnButton = new QPushButton("查看库存预警", box);

    form->addRow("条形码", barcodeEdit);
    form->addRow("药品名称", drugEdit);
    form->addRow("分类", m_categoryBox);
    form->addRow("规格", specificationEdit);
    form->addRow("单位", unitBox);
    form->addRow("售价", priceSpin);
    form->addRow("入库数量", quantitySpin);
    form->addRow("预警库存", warningSpin);
    form->addRow("有效期", expiryEdit);
    form->addRow(inButton, warnButton);

    layout()->addWidget(box);

    connect(warnButton, &QPushButton::clicked, this, [this]() {
        setSearchKeyword({});
        refresh();
        QMessageBox::information(this, "库存预警", "已刷新库存列表，请重点查看库存接近或低于预警库存的药品。");
    });

    connect(barcodeEdit, &QLineEdit::returnPressed, this, [drugEdit, barcodeEdit]() {
        if (drugEdit->text().trimmed().isEmpty()) {
            drugEdit->setFocus();
        }
        barcodeEdit->selectAll();
    });

    connect(inButton, &QPushButton::clicked, this, [this, barcodeEdit, drugEdit, specificationEdit, unitBox, priceSpin, quantitySpin, warningSpin, expiryEdit]() {
        if (drugEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "入库失败", "请输入药品名称。");
            return;
        }

        const QString category = m_categoryBox->currentText().trimmed();
        if (category.isEmpty()) {
            QMessageBox::warning(this, "入库失败", "请输入或选择药品分类。");
            return;
        }
        rememberCategory(m_categoryBox, category);

        common::Request request;
        request.module = "inventory";
        request.action = "inbound";
        request.payload["barcode"] = barcodeEdit->text().trimmed();
        request.payload["drugName"] = drugEdit->text().trimmed();
        request.payload["category"] = category;
        request.payload["specification"] = specificationEdit->text().trimmed();
        request.payload["unit"] = unitBox->currentText().trimmed();
        request.payload["salePrice"] = priceSpin->value();
        request.payload["quantity"] = quantitySpin->value();
        request.payload["warningQuantity"] = warningSpin->value();
        request.payload["expiryDate"] = expiryEdit->date().toString("yyyy-MM-dd");

        if (!this->apiClient()->send(request)) {
            QMessageBox::warning(this, "入库失败", "服务端未连接，请先启动服务端。");
            return;
        }
    });
}

void DrugInventoryPage::rowsUpdated(const QJsonArray& rows)
{
    if (!m_categoryBox) {
        return;
    }

    QSignalBlocker blocker(m_categoryBox);
    for (const auto& item : rows) {
        const QString category = item.toObject().value("分类").toString().trimmed();
        if (!category.isEmpty() && m_categoryBox->findText(category, Qt::MatchFixedString) < 0) {
            m_categoryBox->addItem(category);
        }
    }
    saveCategoriesFromBox(m_categoryBox);
}

} // namespace hospital::client
