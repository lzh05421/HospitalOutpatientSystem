#include "client/pages/Pages.h"

#include "client/ApiClient.h"
#include "client/DepartmentCatalog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSet>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace hospital::client {

SchedulePage::SchedulePage(ApiClient* apiClient, QWidget* parent)
    : ModulePage("医生排班", "维护医生出诊日期、全天号源总数和剩余号源。", "schedule", "list", apiClient, parent)
{
    auto* box = new QGroupBox("新增/调整排班", this);
    auto* form = new QFormLayout(box);

    m_scheduleCategoryBox = new QComboBox(box);
    m_scheduleCategoryBox->setEditable(true);
    m_scheduleCategoryBox->setInsertPolicy(QComboBox::NoInsert);
    m_scheduleCategoryBox->addItems(DepartmentCatalog::categories());

    m_scheduleSpecialtyBox = new QComboBox(box);
    m_scheduleSpecialtyBox->setEditable(true);
    m_scheduleSpecialtyBox->setInsertPolicy(QComboBox::NoInsert);

    m_scheduleDepartmentBox = new QComboBox(box);
    m_scheduleDepartmentBox->setEditable(true);
    m_scheduleDepartmentBox->setInsertPolicy(QComboBox::NoInsert);

    m_scheduleDoctorBox = new QComboBox(box);
    m_scheduleDoctorBox->setEditable(true);
    m_scheduleDoctorBox->addItems({"张明 主任医师", "周宁 主任医师", "刘洋 副主任医师", "陈晓 副主任医师",
                                   "李华 副主任医师", "孙洁 主任医师"});

    auto* dateEdit = new QDateEdit(QDate::currentDate(), box);
    dateEdit->setCalendarPopup(true);
    dateEdit->setMinimumDate(QDate::currentDate());

    auto* quotaSpin = new QSpinBox(box);
    quotaSpin->setRange(1, 100);
    quotaSpin->setValue(30);

    auto* addButton = new QPushButton("保存排班", box);
    auto* disableButton = new QPushButton("停诊选中排班", box);
    auto* reloadDoctorsButton = new QPushButton("刷新医生列表", box);
    auto* smartButton = new QPushButton("智能排班7天", box);
    auto* resetButton = new QPushButton("重新排班", box);
    auto* ruleButton = new QPushButton("长期规则", box);
    auto* unscheduledButton = new QPushButton("查看未排班医生", box);
    disableButton->setEnabled(false);

    form->addRow("门诊大类", m_scheduleCategoryBox);
    form->addRow("专科", m_scheduleSpecialtyBox);
    form->addRow("诊室", m_scheduleDepartmentBox);
    form->addRow("医生", m_scheduleDoctorBox);
    form->addRow("出诊日期", dateEdit);
    form->addRow("号源数量", quotaSpin);
    form->addRow(reloadDoctorsButton, addButton);
    form->addRow(smartButton, resetButton);
    form->addRow(ruleButton);
    form->addRow(unscheduledButton);
    form->addRow(disableButton);

    layout()->addWidget(box);

    connect(m_scheduleCategoryBox, &QComboBox::currentTextChanged,
            this, &SchedulePage::updateScheduleSpecialtyOptions);
    connect(m_scheduleSpecialtyBox, &QComboBox::currentTextChanged,
            this, &SchedulePage::updateScheduleClinicOptions);
    connect(m_scheduleDepartmentBox, &QComboBox::currentTextChanged,
            this, &SchedulePage::updateScheduleDoctorOptions);
    connect(reloadDoctorsButton, &QPushButton::clicked, this, [this]() {
        common::Request request;
        request.module = "doctor";
        request.action = "list";
        this->apiClient()->send(request);
    });
    connect(smartButton, &QPushButton::clicked, this, &SchedulePage::smartSchedule);
    connect(resetButton, &QPushButton::clicked, this, &SchedulePage::resetSchedules);
    connect(ruleButton, &QPushButton::clicked, this, [this]() {
        bool loaded = false;
        const QStringList serverRules = loadServerScheduleRules(&loaded);
        if (!loaded) {
            QMessageBox::warning(this, "读取失败", "无法从服务端读取长期排班规则，请确认服务端已启动且当前账号有排班权限。");
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle("长期不可排班规则");
        dialog.resize(760, 620);

        auto* layout = new QVBoxLayout(&dialog);
        auto* intro = new QLabel("长期规则会自动带入智能排班。推荐固定格式：禁排|医生姓名|周一,周五|原因。每行一条。", &dialog);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto* pickerBox = new QGroupBox("按医生添加规则", &dialog);
        auto* pickerLayout = new QFormLayout(pickerBox);
        auto* doctorSearchEdit = new QLineEdit(pickerBox);
        doctorSearchEdit->setPlaceholderText("按医生姓名、科室、职称筛选");
        auto* doctorRuleBox = new QComboBox(pickerBox);
        doctorRuleBox->setEditable(true);
        auto* weekdayTable = new QTableWidget(1, 5, pickerBox);
        weekdayTable->setHorizontalHeaderLabels({"周一", "周二", "周三", "周四", "周五"});
        weekdayTable->verticalHeader()->setVisible(false);
        weekdayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        weekdayTable->setFixedHeight(72);
        weekdayTable->setSelectionMode(QAbstractItemView::NoSelection);
        weekdayTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        for (int column = 0; column < 5; ++column) {
            auto* item = new QTableWidgetItem("禁排");
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setTextAlignment(Qt::AlignCenter);
            weekdayTable->setItem(0, column, item);
        }
        auto* reasonEdit = new QLineEdit(pickerBox);
        reasonEdit->setPlaceholderText("原因：请假、外院坐诊、培训、会议等");
        auto* appendRuleButton = new QPushButton("添加规则", pickerBox);

        auto rebuildRuleDoctors = [this, doctorRuleBox](const QString& keyword) {
            const QString current = doctorRuleBox->currentText();
            QSignalBlocker blocker(doctorRuleBox);
            doctorRuleBox->clear();
            doctorRuleBox->addItem("全部医生");
            QSet<QString> added;
            for (const auto& item : m_scheduleDoctors) {
                const auto doctor = item.toObject();
                const QString name = doctor.value("医生姓名").toString().trimmed();
                const QString title = doctor.value("职称").toString().trimmed();
                const QString department = doctor.value("所属科室").toString().trimmed();
                if (name.isEmpty()) {
                    continue;
                }
                const QString searchable = QString("%1 %2 %3").arg(name, title, department);
                if (!keyword.trimmed().isEmpty() && !searchable.contains(keyword.trimmed(), Qt::CaseInsensitive)) {
                    continue;
                }
                const QString display = QString("%1 %2 | %3").arg(name, title, department).trimmed();
                if (!added.contains(display)) {
                    doctorRuleBox->addItem(display);
                    added.insert(display);
                }
            }
            const int index = doctorRuleBox->findText(current);
            if (index >= 0) {
                doctorRuleBox->setCurrentIndex(index);
            } else if (!current.trimmed().isEmpty()) {
                doctorRuleBox->setEditText(current);
            }
        };
        rebuildRuleDoctors({});

        pickerLayout->addRow("筛选医生", doctorSearchEdit);
        pickerLayout->addRow("排班人", doctorRuleBox);
        pickerLayout->addRow("禁排星期", weekdayTable);
        pickerLayout->addRow("原因", reasonEdit);
        pickerLayout->addRow(appendRuleButton);
        layout->addWidget(pickerBox);

        auto* edit = new QTextEdit(&dialog);
        edit->setPlainText(serverRules.join("\n"));
        edit->setPlaceholderText(
            "示例：\n"
            "禁排|刘晓民|周一,周五|外院\n"
            "禁排|张明|2026-06-10|请假\n"
            "可排|刘院民|周一,周三,周五|固定出诊");
        layout->addWidget(edit);

        connect(doctorSearchEdit, &QLineEdit::textChanged, &dialog, rebuildRuleDoctors);
        connect(appendRuleButton, &QPushButton::clicked, &dialog, [doctorRuleBox, weekdayTable, reasonEdit, edit]() {
            QString target = doctorRuleBox->currentText().trimmed();
            if (target.contains('|')) {
                target = target.section('|', 0, 0).trimmed();
            }
            if (target.contains(' ')) {
                target = target.section(' ', 0, 0).trimmed();
            }
            if (target.isEmpty()) {
                target = "全部医生";
            }

            QStringList weekdays;
            for (int column = 0; column < weekdayTable->columnCount(); ++column) {
                const auto* item = weekdayTable->item(0, column);
                const auto* header = weekdayTable->horizontalHeaderItem(column);
                if (item && header && item->checkState() == Qt::Checked) {
                    weekdays.append(header->text());
                }
            }
            if (weekdays.isEmpty()) {
                QMessageBox::information(weekdayTable, "请选择星期", "请至少勾选一个周一到周五的禁排日期。");
                return;
            }

            const QString reason = reasonEdit->text().trimmed().isEmpty()
                ? QString("禁排")
                : reasonEdit->text().trimmed();
            const QString rule = QString("禁排|%1|%2|%3").arg(target, weekdays.join(","), reason);
            QString text = edit->toPlainText().trimmed();
            if (!text.isEmpty()) {
                text += "\n";
            }
            text += rule;
            edit->setPlainText(text);
            reasonEdit->clear();
            for (int column = 0; column < weekdayTable->columnCount(); ++column) {
                if (auto* item = weekdayTable->item(0, column)) {
                    item->setCheckState(Qt::Unchecked);
                }
            }
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Save)->setText("保存规则");
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");
        layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, [this, edit, &dialog]() {
            QStringList rules;
            for (const QString& line : edit->toPlainText().split('\n')) {
                const QString rule = line.trimmed();
                if (!rule.isEmpty()) {
                    rules.append(rule);
                }
            }
            QString message;
            if (!saveServerScheduleRules(rules, &message)) {
                QMessageBox::warning(this, "保存失败", message.isEmpty() ? "服务端保存长期规则失败。" : message);
                return;
            }
            QMessageBox::information(this, "已保存", message.isEmpty() ? "长期规则已保存，智能排班时会自动应用。" : message);
            dialog.accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.exec();
    });
    connect(unscheduledButton, &QPushButton::clicked, this, &SchedulePage::showUnscheduledDoctors);
    connect(this->apiClient(), &ApiClient::responseReceived, this, &SchedulePage::onDoctorListReceived);

    connect(addButton, &QPushButton::clicked, this, [this, dateEdit, quotaSpin]() {
        const QString doctorText = m_scheduleDoctorBox->currentText();
        if (doctorText.trimmed().isEmpty()) {
            QMessageBox::warning(this, "保存失败", "请先选择或输入医生。");
            return;
        }

        const QString doctorName = doctorText.section(' ', 0, 0);
        QString department = m_scheduleDepartmentBox->currentText().trimmed();
        if (department.isEmpty()) {
            department = DepartmentCatalog::firstClinic(m_scheduleCategoryBox->currentText(), m_scheduleSpecialtyBox->currentText());
        }
        if (department.isEmpty()) {
            QMessageBox::warning(this, "保存失败", "请先选择门诊大类、专科和诊室。");
            return;
        }

        common::Request request;
        request.module = "schedule";
        request.action = "save";
        request.payload["department"] = department;
        request.payload["doctor"] = doctorName;
        request.payload["title"] = doctorText.section(' ', 1);
        request.payload["date"] = dateEdit->date().toString("yyyy-MM-dd");
        request.payload["period"] = "全天";
        request.payload["quota"] = quotaSpin->value();

        if (!this->apiClient()->send(request)) {
            QMessageBox::warning(this, "保存失败", "服务端未连接，请先启动服务端。");
            return;
        }

        QMessageBox::information(this, "已提交", "排班号源已提交，列表会自动刷新。");
        refresh();
    });

    connect(tableWidget(), &QTableWidget::itemSelectionChanged, this, [this, disableButton]() {
        disableButton->setEnabled(!selectedRowObject().isEmpty());
    });

    connect(disableButton, &QPushButton::clicked, this, [this]() {
        const QJsonObject row = selectedRowObject();
        if (row.isEmpty()) {
            QMessageBox::information(this, "请选择排班", "请先在表格中点击要停诊的排班。");
            return;
        }
        if (QMessageBox::question(this, "确认停诊", "确定要停诊当前选中的排班吗？停诊后患者端不再显示该号源。") != QMessageBox::Yes) {
            return;
        }

        common::Request request;
        request.module = "schedule";
        request.action = "delete";
        request.payload = row;
        this->apiClient()->send(request);
    });

    common::Request request;
    request.module = "doctor";
    request.action = "list";
    this->apiClient()->send(request);
    updateScheduleSpecialtyOptions();
}

common::Response SchedulePage::sendScheduleRequestSync(const QString& action, const QJsonObject& payload) const
{
    common::Response captured;
    bool received = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    const QMetaObject::Connection responseConnection = connect(apiClient(), &ApiClient::responseReceived, &loop,
        [&](const common::Response& response) {
            if (response.data.value("module").toString() != "schedule"
                || response.data.value("action").toString() != action) {
                return;
            }
            captured = response;
            received = true;
            loop.quit();
        });
    const QMetaObject::Connection timeoutConnection = connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    common::Request request;
    request.module = "schedule";
    request.action = action;
    request.payload = payload;
    if (!apiClient()->send(request)) {
        disconnect(responseConnection);
        disconnect(timeoutConnection);
        return {false, "服务端未连接，请先启动服务端。", {}};
    }

    timer.start(5000);
    loop.exec();
    disconnect(responseConnection);
    disconnect(timeoutConnection);

    if (!received) {
        return {false, "等待服务端响应超时。", {}};
    }
    return captured;
}

QStringList SchedulePage::loadServerScheduleRules(bool* ok) const
{
    if (ok) {
        *ok = false;
    }

    const common::Response response = sendScheduleRequestSync("rulesList");
    if (!response.success) {
        return {};
    }

    QStringList rules;
    for (const auto& item : response.data.value("rules").toArray()) {
        const QString rule = item.toString().trimmed();
        if (!rule.isEmpty()) {
            rules.append(rule);
        }
    }
    if (ok) {
        *ok = true;
    }
    return rules;
}

bool SchedulePage::saveServerScheduleRules(const QStringList& rules, QString* message) const
{
    QJsonArray array;
    for (const QString& rule : rules) {
        const QString trimmed = rule.trimmed();
        if (!trimmed.isEmpty()) {
            array.append(trimmed);
        }
    }

    QJsonObject payload;
    payload["rules"] = array;
    const common::Response response = sendScheduleRequestSync("rulesSaveAll", payload);
    if (message) {
        *message = response.message;
    }
    return response.success;
}

QJsonArray SchedulePage::loadServerScheduleRange(const QDate& startDate, const QDate& endDate, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (!startDate.isValid() || !endDate.isValid() || startDate > endDate) {
        return {};
    }

    QJsonObject payload;
    payload["startDate"] = startDate.toString("yyyy-MM-dd");
    payload["endDate"] = endDate.toString("yyyy-MM-dd");
    const common::Response response = sendScheduleRequestSync("rangeList", payload);
    if (!response.success) {
        return {};
    }
    if (ok) {
        *ok = true;
    }
    return response.data.value("rows").toArray();
}

void SchedulePage::rowsUpdated(const QJsonArray& rows)
{
    m_scheduleRows = rows;
}

void SchedulePage::onDoctorListReceived(const common::Response& response)
{
    if (response.data.value("module").toString() != "doctor"
        || response.data.value("action").toString() != "list"
        || !response.success) {
        return;
    }

    m_scheduleDoctors = response.data.value("rows").toArray();

    updateScheduleDoctorOptions();
}

void SchedulePage::updateScheduleSpecialtyOptions()
{
    if (!m_scheduleCategoryBox || !m_scheduleSpecialtyBox) {
        return;
    }

    const QString current = m_scheduleSpecialtyBox->currentText();
    const QStringList specialties = DepartmentCatalog::specialties(m_scheduleCategoryBox->currentText());

    QSignalBlocker blocker(m_scheduleSpecialtyBox);
    m_scheduleSpecialtyBox->clear();
    m_scheduleSpecialtyBox->addItems(specialties);
    const int index = m_scheduleSpecialtyBox->findText(current);
    if (index >= 0) {
        m_scheduleSpecialtyBox->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        m_scheduleSpecialtyBox->setEditText(current);
    }

    updateScheduleClinicOptions();
}

void SchedulePage::updateScheduleClinicOptions()
{
    if (!m_scheduleCategoryBox || !m_scheduleSpecialtyBox || !m_scheduleDepartmentBox) {
        return;
    }

    const QString current = m_scheduleDepartmentBox->currentText();
    const QStringList clinics = DepartmentCatalog::clinics(m_scheduleCategoryBox->currentText(), m_scheduleSpecialtyBox->currentText());

    QSignalBlocker blocker(m_scheduleDepartmentBox);
    m_scheduleDepartmentBox->clear();
    m_scheduleDepartmentBox->addItems(clinics);
    const int index = m_scheduleDepartmentBox->findText(current);
    if (index >= 0) {
        m_scheduleDepartmentBox->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        m_scheduleDepartmentBox->setEditText(current);
    }

    updateScheduleDoctorOptions();
}

void SchedulePage::updateScheduleDoctorOptions()
{
    if (!m_scheduleDoctorBox || !m_scheduleDepartmentBox || !m_scheduleSpecialtyBox) {
        return;
    }

    const QString currentDoctor = m_scheduleDoctorBox->currentText();
    const QString selectedClinic = m_scheduleDepartmentBox->currentText().trimmed();
    const QString selectedSpecialty = m_scheduleSpecialtyBox->currentText().trimmed();
    QStringList doctors;
    for (const auto& item : m_scheduleDoctors) {
        const auto doctor = item.toObject();
        const QString department = doctor.value("所属科室").toString().trimmed();
        if (!selectedClinic.isEmpty() && department != selectedClinic && department != selectedSpecialty) {
            continue;
        }

        const QString name = doctor.value("医生姓名").toString().trimmed();
        const QString title = doctor.value("职称").toString().trimmed();
        const QString display = title.isEmpty() ? name : name + " " + title;
        if (!name.isEmpty() && !doctors.contains(display)) {
            doctors.append(display);
        }
    }
    doctors.sort(Qt::CaseInsensitive);

    QSignalBlocker blocker(m_scheduleDoctorBox);
    m_scheduleDoctorBox->clear();
    m_scheduleDoctorBox->addItems(doctors);
    const int index = m_scheduleDoctorBox->findText(currentDoctor);
    if (index >= 0) {
        m_scheduleDoctorBox->setCurrentIndex(index);
    } else if (!currentDoctor.trimmed().isEmpty() && doctors.isEmpty()) {
        m_scheduleDoctorBox->setEditText(currentDoctor);
    }
}

void SchedulePage::smartSchedule()
{
    if (m_scheduleDoctors.isEmpty()) {
        QMessageBox::information(this, "暂无医生", "请先点击“刷新医生列表”，或在医生管理中维护可排班医生。");
        return;
    }

    bool loadedRules = false;
    const QStringList serverRules = loadServerScheduleRules(&loadedRules);
    if (!loadedRules) {
        QMessageBox::warning(this, "读取失败", "无法从服务端读取长期排班规则，请确认服务端已启动且当前账号有排班权限。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("智能排班条件");
    dialog.resize(760, 620);

    auto* layout = new QVBoxLayout(&dialog);
    auto* intro = new QLabel(
        "上三休一策略："
        "将从开始日期起自动补齐 7 天排班：医生上三天休息一天，休息错开；周六、周日也参与排班；每天每个门诊至少保留一名医生。\n"
        "可以先快速查询并选定医生/科室/职称，再填写详细原因；下面规则框仍可手动补充。", &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* rangeBox = new QGroupBox("排班范围", &dialog);
    auto* rangeLayout = new QFormLayout(rangeBox);
    auto* startDateEdit = new QDateEdit(QDate::currentDate(), rangeBox);
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("yyyy-MM-dd");
    startDateEdit->setMinimumDate(QDate::currentDate());
    rangeLayout->addRow("开始排班日期", startDateEdit);
    layout->addWidget(rangeBox);

    auto* quickBox = new QGroupBox("快速添加条件", &dialog);
    auto* quickLayout = new QFormLayout(quickBox);
    auto* quickSearchEdit = new QLineEdit(quickBox);
    quickSearchEdit->setPlaceholderText("按医生姓名、科室、职称快速查询");
    auto* quickDoctorBox = new QComboBox(quickBox);
    quickDoctorBox->setEditable(true);
    auto* quickDateEdit = new QDateEdit(QDate::currentDate(), quickBox);
    quickDateEdit->setCalendarPopup(true);
    quickDateEdit->setDisplayFormat("yyyy-MM-dd");
    auto* quickScopeBox = new QComboBox(quickBox);
    quickScopeBox->addItems({"仅当天", "每周同一天", "未来7天"});
    auto* quickReasonEdit = new QLineEdit(quickBox);
    quickReasonEdit->setPlaceholderText("详细原因：请假、外院坐诊、培训、手术、会议等");
    auto* addRuleButton = new QPushButton("添加到条件", quickBox);

    auto rebuildQuickDoctors = [this, quickDoctorBox](const QString& keyword) {
        const QString current = quickDoctorBox->currentText();
        QSignalBlocker blocker(quickDoctorBox);
        quickDoctorBox->clear();
        quickDoctorBox->addItem("全部医生");
        QSet<QString> added;
        for (const auto& item : m_scheduleDoctors) {
            const auto doctor = item.toObject();
            const QString name = doctor.value("医生姓名").toString().trimmed();
            const QString title = doctor.value("职称").toString().trimmed();
            const QString department = doctor.value("所属科室").toString().trimmed();
            if (name.isEmpty()) {
                continue;
            }
            const QString searchable = QString("%1 %2 %3").arg(name, title, department);
            if (!keyword.trimmed().isEmpty() && !searchable.contains(keyword.trimmed(), Qt::CaseInsensitive)) {
                continue;
            }
            const QString display = QString("%1 %2 | %3").arg(name, title, department).trimmed();
            if (!added.contains(display)) {
                quickDoctorBox->addItem(display);
                added.insert(display);
            }
        }
        const int index = quickDoctorBox->findText(current);
        if (index >= 0) {
            quickDoctorBox->setCurrentIndex(index);
        }
    };
    rebuildQuickDoctors({});

    quickLayout->addRow("快速查询", quickSearchEdit);
    quickLayout->addRow("对象", quickDoctorBox);

    auto* dateLine = new QHBoxLayout();
    dateLine->addWidget(quickDateEdit);
    dateLine->addWidget(quickScopeBox);
    quickLayout->addRow("条件时间", dateLine);
    quickLayout->addRow("原因", quickReasonEdit);
    quickLayout->addRow(addRuleButton);
    layout->addWidget(quickBox);

    auto* rulesEdit = new QTextEdit(&dialog);
    rulesEdit->setPlainText(serverRules.join("\n"));
    rulesEdit->setPlaceholderText(
        "示例：\n"
        "禁排|刘晓民|周一,周五|外院\n"
        "禁排|张明|2026-06-05|请假\n"
        "可排|刘院民|周一,周三,周五|固定出诊");
    layout->addWidget(rulesEdit);

    auto* hint = new QLabel("推荐固定格式：禁排|医生姓名|周一,周五|原因；可排|医生姓名|周一,周三|原因。", &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#64748b;");
    layout->addWidget(hint);

    connect(quickSearchEdit, &QLineEdit::textChanged, &dialog, rebuildQuickDoctors);
    connect(addRuleButton, &QPushButton::clicked, &dialog, [quickDoctorBox, quickDateEdit, quickScopeBox, quickReasonEdit, rulesEdit]() {
        QString target = quickDoctorBox->currentText().trimmed();
        if (target.contains('|')) {
            target = target.section('|', 0, 0).trimmed();
        }
        if (target.contains(' ')) {
            target = target.section(' ', 0, 0).trimmed();
        }
        if (target == "全部医生") {
            target = "全部医生";
        }

        const QDate date = quickDateEdit->date();
        QString timeText;
        if (quickScopeBox->currentText() == "每周同一天") {
            static const QStringList weekNames = {"", "周一", "周二", "周三", "周四", "周五", "周六", "周日"};
            timeText = weekNames.at(date.dayOfWeek());
        } else if (quickScopeBox->currentText() == "未来7天") {
            timeText = "全部";
        } else {
            timeText = date.toString("yyyy-MM-dd");
        }

        const QString reason = quickReasonEdit->text().trimmed().isEmpty()
            ? QString("禁排")
            : quickReasonEdit->text().trimmed();
        const QString rule = QString("禁排|%1|%2|%3")
            .arg(target.isEmpty() ? QString("全部医生") : target,
                 timeText,
                 reason);
        if (rule.trimmed().isEmpty()) {
            QMessageBox::information(rulesEdit, "请先选择条件", "请选择医生/时间，或填写原因后再添加。");
            return;
        }

        QString text = rulesEdit->toPlainText().trimmed();
        if (!text.isEmpty()) {
            text += "\n";
        }
        text += rule;
        rulesEdit->setPlainText(text);
        quickReasonEdit->clear();
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("开始排班");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList rules;
    for (const QString& line : rulesEdit->toPlainText().split('\n')) {
        const QString rule = line.trimmed();
        if (!rule.isEmpty()) {
            rules.append(rule);
        }
    }
    QString saveRulesMessage;
    if (!saveServerScheduleRules(rules, &saveRulesMessage)) {
        QMessageBox::warning(this, "保存规则失败", saveRulesMessage.isEmpty() ? "服务端保存长期规则失败。" : saveRulesMessage);
        return;
    }

    QJsonArray rows;
    QSet<QString> generatedKeys;
    QSet<QString> generatedCoverageKeys;
    const QDate startDate = startDateEdit->date();
    const QDate endDate = startDate.addDays(6);
    bool loadedScheduleRange = false;
    const QJsonArray existingScheduleRows = loadServerScheduleRange(startDate, endDate, &loadedScheduleRange);
    if (!loadedScheduleRange) {
        QMessageBox::warning(this, "读取排班失败", "无法从服务端读取排班范围数据，已停止智能排班以避免重复号源。");
        return;
    }

    for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
        const QDate date = startDate.addDays(dayOffset);
        const QString dateText = date.toString("yyyy-MM-dd");

        for (const auto& item : m_scheduleDoctors) {
            const auto doctor = item.toObject();
            const QString name = doctor.value("医生姓名").toString().trimmed();
            if (name.isEmpty()) {
                continue;
            }

            const QString clinic = clinicForDoctor(doctor);
            const QStringList clinicDoctors = doctorsForClinic(clinic);
            const QString key = name + "|" + dateText;
            const QString coverageKey = clinic + "|" + dateText;
            bool hasRotationCoverage = false;
            for (const QString& clinicDoctor : clinicDoctors) {
                if (shouldDoctorWorkOnRotation(clinicDoctor, dayOffset, clinicDoctors)
                    && !hasScheduleInRows(existingScheduleRows, clinicDoctor, dateText)
                    && !generatedKeys.contains(clinicDoctor + "|" + dateText)) {
                    hasRotationCoverage = true;
                    break;
                }
            }
            const bool needsClinicCoverage = !hasActiveClinicCoverage(existingScheduleRows, clinic, dateText)
                && !generatedCoverageKeys.contains(coverageKey)
                && !hasRotationCoverage;
            if (hasScheduleInRows(existingScheduleRows, name, dateText) || generatedKeys.contains(key)) {
                if (!hasActiveClinicCoverage(existingScheduleRows, clinic, dateText)) {
                    generatedCoverageKeys.insert(coverageKey);
                }
                continue;
            }
            if (!needsClinicCoverage) {
                if (!shouldDoctorWorkOnRotation(name, dayOffset, clinicDoctors)) {
                    continue;
                }
            }

            QJsonObject row;
            row["department"] = clinic;
            row["doctor"] = name;
            row["title"] = doctor.value("职称").toString("医师");
            row["date"] = dateText;
            row["period"] = "全天";
            row["quota"] = quotaForDoctor(doctor);
            rows.append(row);
            generatedKeys.insert(key);
            generatedCoverageKeys.insert(coverageKey);
        }
    }

    if (rows.isEmpty()) {
        QMessageBox::information(this, "无需排班", "未来 7 天已有排班较完整，没有需要自动补齐的班次。");
        return;
    }

    common::Request request;
    request.module = "schedule";
    request.action = "batchSave";
    request.payload["rows"] = rows;
    if (!apiClient()->send(request)) {
        QMessageBox::warning(this, "智能排班失败", "服务端未连接，请先启动服务端。");
        return;
    }

    QMessageBox::information(this, "智能排班已提交", QString("已生成 %1 条排班，列表会刷新后显示。").arg(rows.size()));
    refresh();
}

void SchedulePage::showUnscheduledDoctors()
{
    if (m_scheduleDoctors.isEmpty()) {
        QMessageBox::information(this, "暂无医生", "请先点击“刷新医生列表”，或在医生管理中维护可排班医生。");
        return;
    }

    QStringList names;
    const QDate today = QDate::currentDate();
    for (const auto& item : m_scheduleDoctors) {
        const auto doctor = item.toObject();
        const QString name = doctor.value("医生姓名").toString().trimmed();
        if (name.isEmpty()) {
            continue;
        }

        bool scheduled = false;
        for (int dayOffset = 0; dayOffset < 7 && !scheduled; ++dayOffset) {
            const QString dateText = today.addDays(dayOffset).toString("yyyy-MM-dd");
            scheduled = hasSchedule(name, dateText);
        }

        if (!scheduled) {
            const QString department = doctor.value("所属科室").toString().trimmed();
            const QString title = doctor.value("职称").toString().trimmed();
            names.append(QString("%1  %2  %3").arg(name, department, title));
        }
    }

    if (names.isEmpty()) {
        QMessageBox::information(this, "未排班医生", "未来 7 天内所有医生都已有排班。");
        return;
    }

    QMessageBox::information(this, "未来7天未排班医生", names.join("\n"));
}

void SchedulePage::resetSchedules()
{
    if (m_scheduleRows.isEmpty()) {
        QMessageBox::information(this, "无需重新排班", "当前没有排班数据。");
        return;
    }

    if (QMessageBox::question(this,
            "确认重新排班",
            "确定要清空当前排班数据吗？清空后患者端不再显示这些号源，可以重新生成或新增排班。")
        != QMessageBox::Yes) {
        return;
    }

    common::Request request;
    request.module = "schedule";
    request.action = "reset";
    if (!apiClient()->send(request)) {
        QMessageBox::warning(this, "重新排班失败", "服务端未连接，请先启动服务端。");
    }
}

bool SchedulePage::hasSchedule(const QString& doctor, const QString& date) const
{
    return hasScheduleInRows(m_scheduleRows, doctor, date);
}

bool SchedulePage::hasScheduleInRows(const QJsonArray& rows, const QString& doctor, const QString& date) const
{
    for (const auto& item : rows) {
        const auto row = item.toObject();
        const QString status = row.value("状态").toVariant().toString().trimmed();
        const bool stopped = status == "0" || status == "停诊" || status == "已停诊" || status == "停用";
        if (row.value("医生").toString() == doctor
            && row.value("出诊日期").toString() == date
            && !stopped) {
            return true;
        }
    }
    return false;
}

bool SchedulePage::hasActiveClinicCoverage(const QJsonArray& rows, const QString& clinic, const QString& date) const
{
    for (const auto& item : rows) {
        const auto row = item.toObject();
        const QString status = row.value("状态").toVariant().toString().trimmed();
        const bool stopped = status == "0" || status == "停诊" || status == "已停诊" || status == "停用";
        if (row.value("科室").toString() == clinic
            && row.value("出诊日期").toString() == date
            && !stopped) {
            return true;
        }
    }
    return false;
}

QStringList SchedulePage::doctorsForClinic(const QString& clinic) const
{
    QStringList doctors;
    for (const auto& item : m_scheduleDoctors) {
        const auto doctor = item.toObject();
        if (clinicForDoctor(doctor) != clinic) {
            continue;
        }
        const QString name = doctor.value("医生姓名").toString().trimmed();
        if (!name.isEmpty() && !doctors.contains(name)) {
            doctors.append(name);
        }
    }
    doctors.sort(Qt::CaseInsensitive);
    return doctors;
}

bool SchedulePage::shouldDoctorWorkOnRotation(const QString& doctor, int dayOffset, const QStringList& clinicDoctors) const
{
    const int index = qMax(0, clinicDoctors.indexOf(doctor));
    const int baseCycleDay = dayOffset % 4;
    const int staggeredCycleDay = (baseCycleDay + index) % 4;
    return staggeredCycleDay != 3;
}

int SchedulePage::scheduledHalfDays(const QString& doctor, const QDate& startDate) const
{
    return scheduledHalfDaysInRows(m_scheduleRows, doctor, startDate);
}

int SchedulePage::scheduledHalfDaysInRows(const QJsonArray& rows, const QString& doctor, const QDate& startDate) const
{
    int count = 0;
    for (const auto& item : rows) {
        const auto row = item.toObject();
        const QDate date = QDate::fromString(row.value("出诊日期").toString(), "yyyy-MM-dd");
        const QString status = row.value("状态").toVariant().toString().trimmed();
        const bool stopped = status == "0" || status == "停诊" || status == "已停诊" || status == "停用";
        if (row.value("医生").toString() == doctor
            && date.isValid()
            && date >= startDate
            && date < startDate.addDays(7)
            && !stopped) {
            ++count;
        }
    }
    return count;
}

QString SchedulePage::clinicForDoctor(const QJsonObject& doctor) const
{
    const QString department = doctor.value("所属科室").toString().trimmed();
    const QString category = DepartmentCatalog::categoryFor(department);
    const QString specialty = DepartmentCatalog::specialtyFor(department);
    const QString clinic = DepartmentCatalog::clinicFor(department);
    if (!clinic.isEmpty()) {
        return clinic;
    }
    if (!category.isEmpty() && !specialty.isEmpty()) {
        return DepartmentCatalog::firstClinic(category, specialty);
    }
    return department.isEmpty() ? QString("待维护诊室") : department;
}

int SchedulePage::quotaForDoctor(const QJsonObject& doctor) const
{
    const QString title = doctor.value("职称").toString();
    if (title.contains("知名") || title.contains("主任")) {
        return 30;
    }
    if (title.contains("副主任")) {
        return 25;
    }
    return 20;
}

} // namespace hospital::client
