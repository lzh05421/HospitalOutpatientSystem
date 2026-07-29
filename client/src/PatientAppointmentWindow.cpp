#include "client/PatientAppointmentWindow.h"

#include "client/ApiClient.h"
#include "client/DepartmentCatalog.h"
#include "client/PatientLoginDialog.h"
#include "client/PatientManager.h"
#include "client/PaymentSelectionDialog.h"
#include "client/UserCenterWindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>

namespace hospital::client {
namespace {

constexpr auto kPendingPaymentStatus = "PENDING_PAYMENT";
constexpr auto kWaitingStatus = "WAITING";

QString maskedIdCard(const QString& idCard)
{
    if (idCard.size() <= 10) {
        return idCard;
    }
    return idCard.left(6) + "********" + idCard.right(4);
}

} // namespace

PatientAppointmentWindow::PatientAppointmentWindow(ApiClient* apiClient,
                                                   PatientManager* patientManager,
                                                   QWidget* parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
    , m_patientManager(patientManager ? patientManager : new PatientManager(apiClient, this))
{
    setWindowTitle("患者预约挂号");
    resize(1180, 760);
    setMinimumSize(980, 640);
    setObjectName("appointmentWindow");

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(14);

    auto* title = new QLabel("患者预约挂号", central);
    title->setObjectName("pageTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* subtitle = new QLabel("先用智能分诊推荐科室，再选择接诊医生和就诊时间，填写就诊人信息后提交预约。", central);
    subtitle->setObjectName("pageDescription");
    subtitle->setWordWrap(true);

    m_currentPatientCard = new QPushButton(central);
    m_currentPatientCard->setObjectName("currentPatientCard");
    m_currentPatientCard->setMinimumHeight(76);
    m_currentPatientCard->setCursor(Qt::PointingHandCursor);
    m_currentPatientCard->setText("当前就诊人\n未选择，点击切换或添加新就诊人");

    m_categoryBox = new QComboBox(central);
    m_categoryBox->setEditable(true);
    m_categoryBox->setInsertPolicy(QComboBox::NoInsert);
    m_categoryBox->addItems(DepartmentCatalog::categories());
    m_specialtyBox = new QComboBox(central);
    m_specialtyBox->setEditable(true);
    m_specialtyBox->setInsertPolicy(QComboBox::NoInsert);
    m_departmentBox = new QComboBox(central);
    m_departmentBox->setEditable(true);
    m_departmentBox->setInsertPolicy(QComboBox::NoInsert);
    m_doctorBox = new QComboBox(central);
    m_doctorBox->setEditable(true);
    m_doctorBox->setInsertPolicy(QComboBox::NoInsert);

    m_dateEdit = new QDateEdit(QDate::currentDate(), central);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setMinimumDate(QDate::currentDate());
    m_dateEdit->setMaximumDate(QDate::currentDate().addDays(14));

    m_timeSlotBox = new QComboBox(central);
    auto* refreshSchedulesButton = new QPushButton("刷新可约号源", central);
    refreshSchedulesButton->setObjectName("primaryButton");

    auto* appointmentBox = new QGroupBox("选择号源", central);
    auto* appointmentForm = new QFormLayout(appointmentBox);
    appointmentForm->addRow("门诊大类", m_categoryBox);
    appointmentForm->addRow("专科", m_specialtyBox);
    appointmentForm->addRow("诊室", m_departmentBox);
    appointmentForm->addRow("选择医生", m_doctorBox);
    appointmentForm->addRow("挂号日期", m_dateEdit);
    appointmentForm->addRow("就诊时间", m_timeSlotBox);
    appointmentForm->addRow("", refreshSchedulesButton);
    appointmentForm->setLabelAlignment(Qt::AlignRight);
    appointmentForm->setHorizontalSpacing(12);
    appointmentForm->setVerticalSpacing(10);

    m_patientNameEdit = new QLineEdit(central);
    m_phoneEdit = new QLineEdit(central);
    m_idCardEdit = new QLineEdit(central);
    m_paymentModeBox = new QComboBox(central);
    m_paymentModeBox->addItems({"自费", "医保统筹"});
    m_emergencyCheckBox = new QCheckBox("急诊挂号", central);
    m_emergencyReasonEdit = new QLineEdit(central);
    m_emergencyReasonEdit->setPlaceholderText("例如：胸痛、高热、外伤、呼吸困难");
    m_insuranceProfileButton = new QPushButton("医保信息", central);
    m_insuranceProfileButton->setObjectName("secondaryButton");
    m_insuranceStatusLabel = new QLabel("当前为自费挂号。", central);
    m_insuranceStatusLabel->setObjectName("insuranceStatusLabel");
    m_insuranceStatusLabel->setWordWrap(true);
    m_symptomEdit = new QTextEdit(central);
    m_symptomEdit->setMinimumHeight(96);
    m_symptomEdit->setPlaceholderText("例如：孩子发烧两天，咳嗽，体温38.5度。");

    auto* recommendButton = new QPushButton("智能分析并推荐科室", central);
    m_addAppointmentButton = new QPushButton("添加就诊人并预约", central);
    recommendButton->setObjectName("secondaryButton");
    m_addAppointmentButton->setObjectName("primaryButton");

    auto* patientBox = new QGroupBox("就诊人信息", central);
    auto* patientForm = new QFormLayout(patientBox);
    auto* paymentRow = new QWidget(patientBox);
    auto* paymentLayout = new QHBoxLayout(paymentRow);
    paymentLayout->setContentsMargins(0, 0, 0, 0);
    paymentLayout->setSpacing(8);
    paymentLayout->addWidget(m_paymentModeBox, 1);
    paymentLayout->addWidget(m_insuranceProfileButton);
    patientForm->addRow("姓名", m_patientNameEdit);
    patientForm->addRow("手机号", m_phoneEdit);
    patientForm->addRow("身份证号", m_idCardEdit);
    patientForm->addRow("", m_emergencyCheckBox);
    patientForm->addRow("急诊原因", m_emergencyReasonEdit);
    patientForm->addRow("支付方式", paymentRow);
    patientForm->addRow("", m_insuranceStatusLabel);
    patientForm->addRow("智能分诊", m_symptomEdit);
    patientForm->addRow(recommendButton, m_addAppointmentButton);
    patientForm->setLabelAlignment(Qt::AlignRight);
    patientForm->setHorizontalSpacing(12);
    patientForm->setVerticalSpacing(10);

    m_resultTable = new QTableWidget(0, 9, central);
    m_resultTable->setHorizontalHeaderLabels({"预约号", "就诊人", "科室", "医生", "日期", "时间段", "状态", "账单号", "操作"});
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->horizontalHeader()->setMinimumSectionSize(98);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setShowGrid(false);
    m_resultTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(m_currentPatientCard);
    auto* forms = new QHBoxLayout();
    forms->setSpacing(14);
    forms->addWidget(appointmentBox);
    forms->addWidget(patientBox);
    root->addLayout(forms);
    auto* recordHeader = new QHBoxLayout();
    auto* recordTitle = new QLabel("预约记录", central);
    recordTitle->setObjectName("sectionTitle");
    auto* historyButton = new QPushButton("查看历史订单", central);
    auto* userCenterButton = new QPushButton("个人中心", central);
    historyButton->setObjectName("secondaryButton");
    userCenterButton->setObjectName("secondaryButton");
    recordHeader->addWidget(recordTitle);
    recordHeader->addStretch();
    recordHeader->addWidget(userCenterButton);
    recordHeader->addWidget(historyButton);
    root->addLayout(recordHeader);
    root->addWidget(m_resultTable, 1);
    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow#appointmentWindow {
            background: #f3f6fb;
        }
        QLabel {
            color: #18212f;
        }
        QLabel#pageTitle {
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#pageDescription {
            color: #667085;
            font-size: 13px;
        }
        QLabel#sectionTitle {
            color: #344054;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#insuranceStatusLabel {
            color: #475467;
            font-size: 13px;
            font-weight: 600;
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
        QPushButton {
            min-height: 34px;
            border-radius: 8px;
            border: 1px solid #cbd5e1;
            background: #ffffff;
            color: #1f2937;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            border-color: #2563eb;
            color: #1d4ed8;
            background: #f8fbff;
        }
        QPushButton#primaryButton {
            background: #2563eb;
            border-color: #2563eb;
            color: #ffffff;
        }
        QPushButton#primaryButton:hover {
            background: #1d4ed8;
            color: #ffffff;
        }
        QPushButton#secondaryButton {
            background: #ffffff;
            color: #334155;
        }
        QPushButton#currentPatientCard {
            background: #ffffff;
            border: 1px solid #bfdbfe;
            border-left: 5px solid #2563eb;
            color: #1e3a8a;
            text-align: left;
            padding: 12px 18px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#currentPatientCard:hover {
            background: #eff6ff;
            border-color: #93c5fd;
        }
        QLineEdit, QComboBox, QDateEdit, QTextEdit {
            min-height: 32px;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            padding: 5px 9px;
            background: #ffffff;
            color: #111827;
        }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTextEdit:focus {
            border-color: #2563eb;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #d8dee8;
            border-radius: 8px;
            gridline-color: #eef2f7;
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

    connect(m_categoryBox, &QComboBox::currentTextChanged, this, &PatientAppointmentWindow::updateSpecialties);
    connect(m_specialtyBox, &QComboBox::currentTextChanged, this, &PatientAppointmentWindow::updateClinics);
    connect(m_departmentBox, &QComboBox::currentTextChanged, this, &PatientAppointmentWindow::updateDoctors);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &PatientAppointmentWindow::updateTimeSlots);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &PatientAppointmentWindow::updateDoctors);
    connect(m_departmentBox, &QComboBox::currentTextChanged, this, [this]() { resetInsuranceCheck(false); });
    connect(m_doctorBox, &QComboBox::currentTextChanged, this, [this]() { resetInsuranceCheck(false); });
    connect(m_dateEdit, &QDateEdit::dateChanged, this, [this]() { resetInsuranceCheck(false); });
    connect(m_timeSlotBox, &QComboBox::currentTextChanged, this, [this]() { resetInsuranceCheck(false); });
    connect(m_paymentModeBox, &QComboBox::currentTextChanged, this, [this](const QString& mode) {
        resetInsuranceCheck(false);
        if (mode == "医保统筹") {
            requestInsurancePrecheck();
        }
    });
    connect(recommendButton, &QPushButton::clicked, this, &PatientAppointmentWindow::recommendDepartment);
    connect(refreshSchedulesButton, &QPushButton::clicked, this, &PatientAppointmentWindow::requestSchedules);
    connect(m_insuranceProfileButton, &QPushButton::clicked, this, &PatientAppointmentWindow::requestInsuranceProfile);
    connect(m_addAppointmentButton, &QPushButton::clicked, this, &PatientAppointmentWindow::addAppointment);
    connect(userCenterButton, &QPushButton::clicked, this, &PatientAppointmentWindow::openUserCenter);
    connect(historyButton, &QPushButton::clicked, this, &PatientAppointmentWindow::requestOrderHistory);
    connect(m_currentPatientCard, &QPushButton::clicked, this, &PatientAppointmentWindow::showPatientSwitcher);
    connect(m_apiClient, &ApiClient::responseReceived, this, &PatientAppointmentWindow::onResponseReceived);
    connect(m_patientManager, &PatientManager::patientsLoaded, this, &PatientAppointmentWindow::refreshCurrentPatientCard);
    connect(m_patientManager, &PatientManager::currentPatientChanged, this, [this]() {
        resetInsuranceCheck(true);
        applyCurrentPatient();
    });
    connect(m_patientManager, &PatientManager::errorOccurred, this, [this](const QString& message) {
        QMessageBox::warning(this, "就诊人", message);
    });

    updateTimeSlots();
    updateDepartments();
    if (ensurePatientLoggedIn()) {
        m_patientManager->loadPatients();
        applyCurrentPatient();
    }
    requestSchedules();
}

void PatientAppointmentWindow::requestSchedules()
{
    common::Request request;
    request.module = "schedule";
    request.action = "list";
    if (!m_apiClient->send(request)) {
        QMessageBox::warning(this, "加载失败", "服务端未连接，请先启动项目。");
    }
}

void PatientAppointmentWindow::updateDepartments()
{
    const QString current = m_categoryBox->currentText();
    const QStringList categories = DepartmentCatalog::categories();

    m_categoryBox->blockSignals(true);
    m_categoryBox->clear();
    m_categoryBox->addItems(categories);
    if (!current.isEmpty()) {
        const int index = m_categoryBox->findText(current);
        if (index >= 0) {
            m_categoryBox->setCurrentIndex(index);
        } else {
            m_categoryBox->setEditText(current);
        }
    }
    m_categoryBox->blockSignals(false);
    updateSpecialties();
    chooseFirstAvailableSchedule();
}

void PatientAppointmentWindow::updateSpecialties()
{
    const QString current = m_specialtyBox->currentText();
    const QStringList specialties = DepartmentCatalog::specialties(m_categoryBox->currentText());

    m_specialtyBox->blockSignals(true);
    m_specialtyBox->clear();
    m_specialtyBox->addItems(specialties);
    const int index = m_specialtyBox->findText(current);
    if (index >= 0) {
        m_specialtyBox->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        m_specialtyBox->setEditText(current);
    }
    m_specialtyBox->blockSignals(false);
    updateClinics();
}

void PatientAppointmentWindow::updateClinics()
{
    const QString current = m_departmentBox->currentText();
    const QStringList clinics = DepartmentCatalog::clinics(m_categoryBox->currentText(), m_specialtyBox->currentText());

    m_departmentBox->blockSignals(true);
    m_departmentBox->clear();
    if (!clinics.isEmpty()) {
        m_departmentBox->addItem("全部诊室");
    }
    m_departmentBox->addItems(clinics);
    const int index = m_departmentBox->findText(current);
    if (index >= 0) {
        m_departmentBox->setCurrentIndex(index);
    } else if (!current.isEmpty() && current != "全部诊室") {
        m_departmentBox->setEditText(current);
    }
    m_departmentBox->blockSignals(false);
    updateDoctors();
}

void PatientAppointmentWindow::updateDoctors()
{
    if (m_autoSelectingSchedule) {
        return;
    }

    m_doctorBox->clear();
    const QString department = selectedClinic();
    const QDate selectedDate = m_dateEdit->date();
    const QString date = selectedDate.toString("yyyy-MM-dd");
    if (firstUsableTimeSlot(selectedDate).isEmpty()) {
        if (chooseFirstAvailableScheduleForDepartment(department)) {
            return;
        }
        m_doctorBox->addItem("当前日期已过或无可预约医生，请选择后续日期");
        return;
    }

    for (const auto& item : m_schedules) {
        const auto schedule = item.toObject();
        if (!scheduleMatchesDepartment(schedule, department)
            || schedule.value("出诊日期").toString() != date
            || scheduleRemain(schedule) <= 0) {
            continue;
        }

        const QString scheduleDepartment = schedule.value("科室").toString();
        const bool showingSpecialty = m_departmentBox->currentText() == "全部诊室";
        const QString display = showingSpecialty
            ? QString("%1 %2 - %3（余%4）")
                .arg(schedule.value("医生").toString(),
                     schedule.value("职称").toString(),
                     scheduleDepartment)
                .arg(scheduleRemain(schedule))
            : QString("%1 %2（余%3）")
                .arg(schedule.value("医生").toString(),
                     schedule.value("职称").toString())
                .arg(scheduleRemain(schedule));
        if (m_doctorBox->findText(display) < 0) {
            m_doctorBox->addItem(display, scheduleDepartment);
        }
    }

    if (m_doctorBox->count() == 0) {
        if (chooseFirstAvailableScheduleForDepartment(department)) {
            return;
        }
        m_doctorBox->addItem("暂无可预约医生，请先在医生排班中给该科室医生设置号源");
    }
}

void PatientAppointmentWindow::updateTimeSlots()
{
    const QString previous = m_timeSlotBox->currentText();
    m_timeSlotBox->clear();
    const QStringList timeSlots = {
        "08:30-09:00", "09:00-09:30", "09:30-10:00", "10:00-10:30", "10:30-11:00",
        "13:30-14:00", "14:00-14:30", "14:30-15:00", "15:00-15:30", "15:30-16:00",
        "16:00-16:30", "16:30-17:00", "17:00-17:30", "17:30-18:00"
    };

    const bool isToday = m_dateEdit->date() == QDate::currentDate();
    const QTime now = QTime::currentTime();
    for (const QString& slot : timeSlots) {
        const QTime end = QTime::fromString(slot.section('-', 1, 1), "HH:mm");
        if (isToday && end.isValid() && end <= now) {
            continue;
        }
        m_timeSlotBox->addItem(slot);
    }

    if (m_timeSlotBox->count() == 0) {
        m_timeSlotBox->addItem("当前时段已过，请选择后续时段");
        return;
    }

    const int previousIndex = m_timeSlotBox->findText(previous);
    if (previousIndex >= 0) {
        m_timeSlotBox->setCurrentIndex(previousIndex);
    }
}

void PatientAppointmentWindow::resetInsuranceCheck(bool switchToSelfPay)
{
    m_insuranceToken.clear();
    m_insuranceDataVersion.clear();
    m_insuranceResultCode.clear();
    m_isInsuranceChecking = false;
    if (switchToSelfPay && m_paymentModeBox) {
        const QSignalBlocker blocker(m_paymentModeBox);
        m_paymentModeBox->setCurrentText("自费");
    }
    if (m_paymentModeBox && m_paymentModeBox->currentText() == "医保统筹") {
        m_insuranceStatusLabel->setText("医保统筹未校验，请重新选择医保统筹完成挂号前置校验。");
        if (m_addAppointmentButton) {
            m_addAppointmentButton->setEnabled(false);
        }
        return;
    }
    if (m_insuranceStatusLabel) {
        m_insuranceStatusLabel->setText("当前为自费挂号。");
    }
    if (m_paymentModeBox) {
        m_paymentModeBox->setEnabled(true);
    }
    if (m_insuranceProfileButton) {
        m_insuranceProfileButton->setEnabled(true);
    }
    if (m_addAppointmentButton) {
        m_addAppointmentButton->setEnabled(true);
    }
}

void PatientAppointmentWindow::setInsuranceChecking(bool checking)
{
    m_isInsuranceChecking = checking;
    if (m_paymentModeBox) {
        m_paymentModeBox->setEnabled(!checking);
    }
    if (m_insuranceProfileButton) {
        m_insuranceProfileButton->setEnabled(!checking);
    }
    if (m_addAppointmentButton) {
        m_addAppointmentButton->setEnabled(!checking);
    }
    if (m_insuranceStatusLabel && checking) {
        m_insuranceStatusLabel->setText("医保资格校验中，请稍候...");
    }
}

void PatientAppointmentWindow::requestInsurancePrecheck()
{
    if (!ensurePatientLoggedIn()) {
        resetInsuranceCheck(true);
        return;
    }
    applyCurrentPatient();
    const PatientProfile currentPatient = m_patientManager->currentPatient();
    const QString selectedPatientId = currentPatient.patientId.isEmpty()
        ? m_apiClient->patientId()
        : currentPatient.patientId;
    if (selectedPatientId.isEmpty()
        || selectedAppointmentDepartment().isEmpty()
        || doctorNameFromDisplay(m_doctorBox->currentText()).isEmpty()
        || !m_timeSlotBox->currentText().contains('-')) {
        resetInsuranceCheck(true);
        QMessageBox::warning(this, "医保统筹", "请先确定就诊人、科室、医生、日期和号源后再选择医保统筹。");
        return;
    }

    setInsuranceChecking(true);
    common::Request request;
    request.module = "registration";
    request.action = "insurancePrecheck";
    request.payload["patientId"] = selectedPatientId;
    request.payload["idCard"] = m_idCardEdit->text().trimmed();
    request.payload["hospitalAreaCode"] = "110100";
    request.payload["department"] = selectedAppointmentDepartment();
    request.payload["doctor"] = doctorNameFromDisplay(m_doctorBox->currentText());
    request.payload["date"] = m_dateEdit->date().toString("yyyy-MM-dd");
    request.payload["timeSlot"] = m_timeSlotBox->currentText();
    if (!m_apiClient->send(request)) {
        setInsuranceChecking(false);
        resetInsuranceCheck(true);
        QMessageBox::warning(this, "医保统筹", "服务端未连接，本次挂号无法使用医保统筹，请转为自费。");
    }
}

void PatientAppointmentWindow::requestInsuranceProfile()
{
    if (!ensurePatientLoggedIn()) {
        return;
    }
    applyCurrentPatient();
    const PatientProfile currentPatient = m_patientManager->currentPatient();
    const QString selectedPatientId = currentPatient.patientId.isEmpty()
        ? m_apiClient->patientId()
        : currentPatient.patientId;
    if (selectedPatientId.isEmpty()) {
        QMessageBox::warning(this, "医保信息", "请先选择或添加就诊人后再维护医保信息。");
        showPatientSwitcher();
        return;
    }

    common::Request request;
    request.module = "registration";
    request.action = "insuranceProfile";
    request.payload["patientId"] = selectedPatientId;
    if (!m_apiClient->send(request)) {
        QMessageBox::warning(this, "医保信息", "服务端未连接，请先启动服务端。");
    }
}

void PatientAppointmentWindow::openInsuranceProfileDialog(const QJsonObject& profile)
{
    const PatientProfile currentPatient = m_patientManager->currentPatient();
    const QString selectedPatientId = currentPatient.patientId.isEmpty()
        ? m_apiClient->patientId()
        : currentPatient.patientId;
    if (selectedPatientId.isEmpty()) {
        QMessageBox::warning(this, "医保信息", "请先选择就诊人后再维护医保信息。");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("我的医保信息");
    dialog.setModal(true);
    dialog.resize(460, 420);

    auto* form = new QFormLayout(&dialog);
    auto* profileStatus = new QLabel(profile.value("found").toBool()
                                         ? "已读取医保信息，可修改后保存。"
                                         : "未绑定医保信息，请先填写并保存。", &dialog);
    profileStatus->setWordWrap(true);
    auto* patientLabel = new QLabel(QString("%1  %2")
                                        .arg(currentPatient.name.isEmpty() ? m_patientNameEdit->text() : currentPatient.name,
                                             maskedIdCard(currentPatient.idCard.isEmpty() ? m_idCardEdit->text() : currentPatient.idCard)),
                                    &dialog);
    auto* areaModeBox = new QComboBox(&dialog);
    areaModeBox->addItems({"本地", "异地"});
    areaModeBox->setCurrentText(profile.value("insuredAreaMode").toString("本地"));
    auto* insuranceTypeBox = new QComboBox(&dialog);
    insuranceTypeBox->addItems({"职工医保", "居民医保", "仅住院险", "工伤保险"});
    insuranceTypeBox->setCurrentText(profile.value("insuranceType").toString("居民医保"));
    auto* validEndDateEdit = new QDateEdit(&dialog);
    validEndDateEdit->setCalendarPopup(true);
    validEndDateEdit->setMinimumDate(QDate::currentDate().addYears(-5));
    validEndDateEdit->setMaximumDate(QDate::currentDate().addYears(10));
    const QDate savedValidEndDate = QDate::fromString(profile.value("validEndDate").toString(), "yyyy-MM-dd");
    validEndDateEdit->setDate(savedValidEndDate.isValid() ? savedValidEndDate : QDate::currentDate().addYears(1));
    auto* remoteFiledCheck = new QCheckBox("已办理异地就医备案", &dialog);
    remoteFiledCheck->setChecked(profile.value("remoteFiled").toBool(false));
    auto* arrearsCheck = new QCheckBox("存在欠费停保", &dialog);
    arrearsCheck->setChecked(profile.value("arrearsSuspended").toBool(false));
    auto* quotaTotalSpin = new QDoubleSpinBox(&dialog);
    quotaTotalSpin->setRange(0.0, 999999.0);
    quotaTotalSpin->setDecimals(2);
    quotaTotalSpin->setSuffix(" 元");
    quotaTotalSpin->setValue(profile.value("annualQuotaTotal").toDouble(2000.0));
    auto* quotaUsedSpin = new QDoubleSpinBox(&dialog);
    quotaUsedSpin->setRange(0.0, 999999.0);
    quotaUsedSpin->setDecimals(2);
    quotaUsedSpin->setSuffix(" 元");
    quotaUsedSpin->setValue(profile.value("annualQuotaUsed").toDouble(0.0));
    auto* saveButton = new QPushButton("保存医保信息", &dialog);
    saveButton->setObjectName("primaryButton");

    form->addRow("", profileStatus);
    form->addRow("就诊人", patientLabel);
    form->addRow("参保地区", areaModeBox);
    form->addRow("险种类型", insuranceTypeBox);
    form->addRow("有效期至", validEndDateEdit);
    form->addRow("异地备案", remoteFiledCheck);
    form->addRow("欠费停保", arrearsCheck);
    form->addRow("年度额度", quotaTotalSpin);
    form->addRow("已用额度", quotaUsedSpin);
    form->addRow("", saveButton);

    connect(saveButton, &QPushButton::clicked, &dialog, [this, &dialog, selectedPatientId, areaModeBox, insuranceTypeBox,
                                                          validEndDateEdit, remoteFiledCheck, arrearsCheck,
                                                          quotaTotalSpin, quotaUsedSpin]() {
        if (quotaUsedSpin->value() > quotaTotalSpin->value() && quotaTotalSpin->value() > 0.0) {
            QMessageBox::warning(&dialog, "医保信息", "已用额度不能大于年度额度。");
            return;
        }
        QJsonObject payload;
        payload["patientId"] = selectedPatientId;
        payload["hospitalAreaCode"] = "110100";
        payload["insuredAreaMode"] = areaModeBox->currentText();
        payload["insuranceType"] = insuranceTypeBox->currentText();
        payload["validEndDate"] = validEndDateEdit->date().toString("yyyy-MM-dd");
        payload["remoteFiled"] = remoteFiledCheck->isChecked();
        payload["arrearsSuspended"] = arrearsCheck->isChecked();
        payload["annualQuotaTotal"] = quotaTotalSpin->value();
        payload["annualQuotaUsed"] = quotaUsedSpin->value();
        saveInsuranceProfile(payload);
        dialog.accept();
    });

    dialog.exec();
}

void PatientAppointmentWindow::saveInsuranceProfile(const QJsonObject& payload)
{
    common::Request request;
    request.module = "registration";
    request.action = "saveInsuranceProfile";
    request.payload = payload;
    if (!m_apiClient->send(request)) {
        QMessageBox::warning(this, "医保信息", "服务端未连接，请先启动服务端。");
    }
}

void PatientAppointmentWindow::recommendDepartment()
{
    const QString text = m_symptomEdit->toPlainText();
    const QString department = DepartmentCatalog::recommendedClinicForSymptoms(text);
    selectDepartmentPath(department);

    QString level = "普通";
    QString advice = "建议按普通门诊预约，并携带既往病历。";
    if (text.contains("胸痛") || text.contains("呼吸困难") || text.contains("昏迷")) {
        level = "紧急";
        advice = "建议立即前往急诊，不要等待普通预约。";
    } else if (text.contains("发热") || text.contains("咳嗽") || text.contains("哮喘") || text.contains("呼吸")) {
        advice = "建议记录体温变化、咳痰情况和已用药情况。";
    } else if (text.contains("腹痛") || text.contains("腹泻") || text.contains("呕吐")) {
        advice = "若疼痛剧烈、黑便或呕血，请优先急诊。";
    } else if (text.contains("儿童") || text.contains("孩子") || text.contains("小孩")) {
        advice = "建议记录体温变化、精神状态、饮食和用药情况。";
    }

    QMessageBox::information(this, "智能分诊",
                             QString("推荐科室：%1\n紧急程度：%2\n挂号提示：%3")
                                 .arg(department, level, advice));
}

void PatientAppointmentWindow::addAppointment()
{
    if (!ensurePatientLoggedIn()) {
        return;
    }
    applyCurrentPatient();
    const PatientProfile currentPatient = m_patientManager->currentPatient();
    const QString selectedPatientId = currentPatient.patientId.isEmpty()
        ? m_apiClient->patientId()
        : currentPatient.patientId;
    if (selectedPatientId.isEmpty()) {
        QMessageBox::warning(this, "请选择就诊人", "请先选择或添加就诊人后再预约。");
        showPatientSwitcher();
        return;
    }
    if (m_patientNameEdit->text().trimmed().isEmpty() || m_phoneEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "信息不完整", "请至少填写就诊人姓名和手机号。");
        return;
    }
    if (m_doctorBox->currentText().startsWith("暂无")) {
        QMessageBox::warning(this, "预约失败", "当前科室和日期暂无可预约医生。请点“刷新可约号源”，或更换日期。");
        return;
    }
    if (!m_timeSlotBox->currentText().contains('-')) {
        QMessageBox::warning(this, "预约失败", "当前日期和时段没有可预约时间，请选择系统自动定位到的后续号源。");
        return;
    }
    const bool insuranceSelected = m_paymentModeBox && m_paymentModeBox->currentText() == "医保统筹";
    if (m_isInsuranceChecking) {
        QMessageBox::information(this, "医保统筹", "医保资格校验中，请稍候再确认挂号。");
        return;
    }
    if (insuranceSelected && m_insuranceToken.isEmpty()) {
        requestInsurancePrecheck();
        return;
    }
    const bool isEmergency = m_emergencyCheckBox && m_emergencyCheckBox->isChecked();
    const QString emergencyReason = m_emergencyReasonEdit ? m_emergencyReasonEdit->text().trimmed() : QString();
    if (isEmergency && emergencyReason.isEmpty()) {
        QMessageBox::warning(this, "急诊原因不能为空", "选择急诊挂号后请填写急诊原因。");
        return;
    }

    const QString appointmentNo = QString("AP%1%2")
        .arg(QDate::currentDate().toString("yyyyMMdd"))
        .arg(m_resultTable->rowCount() + 1, 4, 10, QChar('0'));
    m_pendingAppointment = {
        appointmentNo,
        m_patientNameEdit->text(),
        selectedAppointmentDepartment(),
        m_doctorBox->currentText(),
        m_dateEdit->date().toString("yyyy-MM-dd"),
        m_timeSlotBox->currentText()
    };

    common::Request request;
    request.module = "registration";
    request.action = "create";
    request.payload["patientName"] = m_patientNameEdit->text().trimmed();
    request.payload["phone"] = m_phoneEdit->text().trimmed();
    request.payload["idCard"] = m_idCardEdit->text().trimmed();
    request.payload["patientId"] = selectedPatientId;
    request.payload["department"] = selectedAppointmentDepartment();
    request.payload["doctor"] = doctorNameFromDisplay(m_doctorBox->currentText());
    request.payload["date"] = m_dateEdit->date().toString("yyyy-MM-dd");
    request.payload["period"] = "全天";
    request.payload["timeSlot"] = m_timeSlotBox->currentText();
    request.payload["fee"] = 0.0;
    request.payload["paymentMethod"] = insuranceSelected ? "医保统筹" : "自费";
    request.payload["hospitalAreaCode"] = "110100";
    request.payload["isEmergency"] = isEmergency;
    request.payload["emergencyReason"] = emergencyReason;
    if (insuranceSelected) {
        request.payload["insuranceToken"] = m_insuranceToken;
        request.payload["insuranceDataVersion"] = m_insuranceDataVersion;
    }

    if (!m_apiClient->send(request)) {
        QMessageBox::warning(this, "预约失败", "服务端未连接，请先启动服务端。");
        m_pendingAppointment.clear();
        m_pendingRegistrationInsuranceApproved = false;
    }
}

void PatientAppointmentWindow::onResponseReceived(const common::Response& response)
{
    if (response.data.value("module").toString() == "schedule"
        && response.data.value("action").toString() == "list"
        && response.success) {
        m_schedules = response.data.value("rows").toArray();
        updateDepartments();
        return;
    }

    if (response.data.value("module").toString() == "registration"
        && response.data.value("action").toString() == "insuranceProfile") {
        if (!response.success) {
            QMessageBox::warning(this, "医保信息", response.message);
            return;
        }
        openInsuranceProfileDialog(response.data);
        return;
    }

    if (response.data.value("module").toString() == "registration"
        && response.data.value("action").toString() == "saveInsuranceProfile") {
        if (!response.success) {
            QMessageBox::warning(this, "医保信息", response.message);
            return;
        }
        resetInsuranceCheck(false);
        if (m_insuranceStatusLabel) {
            m_insuranceStatusLabel->setText("医保信息已保存，可选择医保统筹进行挂号资格校验。");
        }
        QMessageBox::information(this, "医保信息", response.message);
        if (m_paymentModeBox && m_paymentModeBox->currentText() == "医保统筹") {
            requestInsurancePrecheck();
        }
        return;
    }

    if (response.data.value("module").toString() == "registration"
        && response.data.value("action").toString() == "insurancePrecheck") {
        setInsuranceChecking(false);
        const QString resultCode = response.data.value("resultCode").toString();
        m_insuranceResultCode = resultCode;
        if (response.success) {
            m_insuranceToken = response.data.value("insuranceToken").toString();
            m_insuranceDataVersion = response.data.value("dataVersion").toString();
            if (m_insuranceStatusLabel) {
                m_insuranceStatusLabel->setText(QString("医保统筹资格已通过，挂号费预览：%1。")
                                                     .arg(response.data.value("feePreviewEnabled").toBool()
                                                              ? "可使用医保统筹身份"
                                                              : "未启用"));
            }
            if (m_paymentModeBox) {
                m_paymentModeBox->setEnabled(true);
            }
            if (m_addAppointmentButton) {
                m_addAppointmentButton->setEnabled(true);
            }
        } else {
            resetInsuranceCheck(true);
            QMessageBox::warning(this, "医保统筹",
                                 QString("%1\n\n错误码：%2\n本次挂号无法使用医保统筹，请转为自费挂号。")
                                     .arg(response.message, resultCode));
        }
        return;
    }

    if (response.data.value("module").toString() == "registration"
        && response.data.value("action").toString() == "history") {
        if (!response.success) {
            QMessageBox::warning(this, "历史订单", response.message);
            return;
        }
        populateHistoryRows(response.data.value("rows").toArray());
        return;
    }

    if (response.data.value("module").toString() != "registration"
        || response.data.value("action").toString() != "create") {
        return;
    }

    if (response.success) {
        if (!m_pendingAppointment.isEmpty()) {
            const QString registrationNo = response.data.value("registrationNo").toString();
            if (!registrationNo.isEmpty()) {
                m_pendingAppointment[0] = registrationNo;
            }
        }
        m_pendingRegistrationInsuranceApproved =
            response.data.value("paymentIdentity").toString() == "医保统筹资格已通过"
            || response.data.value("insuranceResultCode").toString() == "REG_INS_000";
        if (!m_insuranceToken.isEmpty()) {
            resetInsuranceCheck(true);
        }
        const QString status = response.data.value("status").toString();
        if (status == kPendingPaymentStatus) {
            const QString billNo = response.data.value("billNo").toString();
            const double totalAmount = response.data.value("totalAmount").toVariant().toDouble();
            const double originalAmount = response.data.value("originalAmount").toVariant().toDouble();
            const double reimbursementAmount = response.data.value("insuranceReimbursementAmount").toVariant().toDouble();
            m_pendingBillNo = billNo;
            m_pendingPaymentToken = response.data.value("paymentToken").toString();
            m_pendingBillAmount = totalAmount;
            QMessageBox paymentBox(this);
            paymentBox.setIcon(QMessageBox::Information);
            paymentBox.setWindowTitle("待支付");
            const QString amountText = reimbursementAmount > 0.0
                ? QString("原挂号费：%1 元\n医保报销：%2 元\n个人应付：%3 元")
                      .arg(originalAmount, 0, 'f', 2)
                      .arg(reimbursementAmount, 0, 'f', 2)
                      .arg(totalAmount, 0, 'f', 2)
                : QString("应付金额：%1 元").arg(totalAmount, 0, 'f', 2);
            paymentBox.setText(QString("%1\n\n账单号：%2\n%3")
                                   .arg(response.message, billNo, amountText));
            auto* payButton = paymentBox.addButton("去支付", QMessageBox::AcceptRole);
            paymentBox.addButton("暂不支付", QMessageBox::RejectRole);
            paymentBox.exec();
            if (paymentBox.clickedButton() == payButton) {
                showPaymentSelectionDialog(billNo, totalAmount);
            } else {
                QMessageBox::information(this, "待支付", "订单已创建，支付完成后才会进入候诊队列。");
                m_pendingAppointment.clear();
                m_pendingBillNo.clear();
                m_pendingPaymentToken.clear();
                m_pendingBillAmount = 0.0;
                m_pendingRegistrationInsuranceApproved = false;
            }
            return;
        }

        appendAppointmentRow(m_pendingAppointment);
        QMessageBox::information(this, "预约成功", response.message);
    } else {
        const QString resultCode = response.data.value("resultCode").toString();
        if (resultCode == "REG_INS_007") {
            m_insuranceToken.clear();
            m_insuranceDataVersion.clear();
            m_insuranceResultCode = resultCode;
            if (m_insuranceStatusLabel) {
                m_insuranceStatusLabel->setText("医保资格信息已更新，请重新进行挂号医保统筹校验。");
            }
            if (m_paymentModeBox) {
                m_paymentModeBox->setCurrentText("医保统筹");
                m_paymentModeBox->setEnabled(true);
            }
            if (m_addAppointmentButton) {
                m_addAppointmentButton->setEnabled(true);
            }
        } else if (resultCode.startsWith("REG_INS_")) {
            resetInsuranceCheck(true);
        }
        QMessageBox::warning(this, "预约失败", response.message);
    }
    m_pendingAppointment.clear();
    m_pendingPaymentToken.clear();
    m_pendingRegistrationInsuranceApproved = false;
}

bool PatientAppointmentWindow::ensurePatientLoggedIn()
{
    if (m_apiClient->isPatientLoggedIn()) {
        return true;
    }

    QMessageBox::information(this, "请先登录", "请先登录患者账号，再继续预约挂号。");
    PatientLoginDialog loginDialog(m_apiClient, this);
    if (loginDialog.exec() != QDialog::Accepted || !m_apiClient->isPatientLoggedIn()) {
        return false;
    }
    m_patientManager->loadPatients();
    applyCurrentPatient();
    return true;
}

void PatientAppointmentWindow::applyCurrentPatient()
{
    if (!m_apiClient->isPatientLoggedIn()) {
        return;
    }
    const PatientProfile currentPatient = m_patientManager->currentPatient();
    const QString patientName = currentPatient.name.isEmpty() ? m_apiClient->patientName() : currentPatient.name;
    const QString patientPhone = currentPatient.phone.isEmpty() ? m_apiClient->patientPhone() : currentPatient.phone;
    const QString patientIdCard = currentPatient.idCard.isEmpty() ? m_apiClient->patientIdCard() : currentPatient.idCard;
    m_patientNameEdit->setText(patientName);
    m_phoneEdit->setText(patientPhone);
    m_idCardEdit->setText(patientIdCard);
    m_patientNameEdit->setReadOnly(true);
    m_phoneEdit->setReadOnly(true);
    m_idCardEdit->setReadOnly(true);
    refreshCurrentPatientCard();
}

void PatientAppointmentWindow::refreshCurrentPatientCard()
{
    if (!m_currentPatientCard) {
        return;
    }

    const PatientProfile currentPatient = m_patientManager->currentPatient();
    if (currentPatient.patientId.isEmpty()) {
        m_currentPatientCard->setText("当前就诊人\n未选择，点击切换或添加新就诊人");
        return;
    }

    const QString tail = currentPatient.idCard.size() >= 4 ? currentPatient.idCard.right(4) : "----";
    m_currentPatientCard->setText(QString("当前就诊人  %1\n%2  身份证后四位 %3，点击切换或添加新就诊人")
                                      .arg(currentPatient.name,
                                           currentPatient.relationship.isEmpty() ? "就诊人" : currentPatient.relationship,
                                           tail));
}

void PatientAppointmentWindow::requestOrderHistory()
{
    if (!ensurePatientLoggedIn()) {
        return;
    }

    common::Request request;
    request.module = "registration";
    request.action = "history";
    request.payload["patientUserId"] = m_apiClient->patientUserId();
    if (m_patientManager->hasCurrentPatient()) {
        request.payload["patientId"] = m_patientManager->currentPatient().patientId;
    }
    if (!m_apiClient->send(request)) {
        QMessageBox::warning(this, "历史订单", "服务端未连接，请先启动服务端。");
    }
}

void PatientAppointmentWindow::openUserCenter()
{
    if (!ensurePatientLoggedIn()) {
        return;
    }

    auto* userCenter = new UserCenterWindow(m_apiClient, m_patientManager, this);
    userCenter->setAttribute(Qt::WA_DeleteOnClose);
    userCenter->show();
}

void PatientAppointmentWindow::showPatientSwitcher()
{
    if (!ensurePatientLoggedIn()) {
        return;
    }

    if (m_patientManager->patients().isEmpty()) {
        m_patientManager->loadPatients();
    }

    QMenu menu(this);
    for (const auto& patient : m_patientManager->patients()) {
        const QString tail = patient.idCard.size() >= 4 ? patient.idCard.right(4) : "----";
        auto* action = menu.addAction(QString("%1  %2  身份证后四位%3")
                                          .arg(patient.name,
                                               patient.relationship.isEmpty() ? "就诊人" : patient.relationship,
                                               tail));
        action->setData(patient.patientId);
    }
    if (!m_patientManager->patients().isEmpty()) {
        menu.addSeparator();
    }
    auto* addAction = menu.addAction("添加新就诊人");

    const QAction* selected = menu.exec(QCursor::pos());
    if (!selected) {
        return;
    }
    if (selected == addAction) {
        addNewPatient();
        return;
    }
    if (m_patientManager->selectPatient(selected->data().toString())) {
        applyCurrentPatient();
    }
}

void PatientAppointmentWindow::addNewPatient()
{
    QDialog dialog(this);
    dialog.setWindowTitle("添加新就诊人");
    dialog.setModal(true);
    dialog.resize(420, 300);

    auto* form = new QFormLayout(&dialog);
    auto* nameEdit = new QLineEdit(&dialog);
    auto* phoneEdit = new QLineEdit(&dialog);
    auto* idCardEdit = new QLineEdit(&dialog);
    auto* genderBox = new QComboBox(&dialog);
    auto* relationshipBox = new QComboBox(&dialog);
    auto* saveButton = new QPushButton("保存就诊人", &dialog);
    genderBox->addItems({"未知", "男", "女"});
    relationshipBox->addItems({"本人", "父母", "配偶", "子女", "家属"});

    form->addRow("姓名", nameEdit);
    form->addRow("手机号", phoneEdit);
    form->addRow("身份证号", idCardEdit);
    form->addRow("性别", genderBox);
    form->addRow("关系", relationshipBox);
    form->addRow("", saveButton);

    connect(saveButton, &QPushButton::clicked, &dialog, [this, &dialog, nameEdit, phoneEdit, idCardEdit, genderBox, relationshipBox]() {
        const QString idCard = idCardEdit->text().trimmed();
        if (!QRegularExpression(QStringLiteral("^\\d{17}[0-9Xx]$")).match(idCard).hasMatch()) {
            QMessageBox::warning(&dialog, "添加失败", "身份证号格式不正确。");
            return;
        }
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "添加失败", "请填写就诊人姓名。");
            return;
        }
        m_patientManager->addPatient(nameEdit->text(),
                                     phoneEdit->text(),
                                     idCard,
                                     genderBox->currentText(),
                                     relationshipBox->currentText());
        dialog.accept();
    });

    dialog.exec();
}

void PatientAppointmentWindow::showPaymentSelectionDialog(const QString& billNo, double totalAmount)
{
    m_pendingBillNo = billNo;
    m_pendingBillAmount = totalAmount;

    PaymentSelectionDialog dialog(m_apiClient,
                                  billNo,
                                  totalAmount,
                                  m_pendingPaymentToken,
                                  m_pendingRegistrationInsuranceApproved,
                                  this);
    bool handled = false;
    connect(&dialog, &PaymentSelectionDialog::paymentCompleted, this, [this, &handled](const QString&) {
        handled = true;
        if (!m_pendingAppointment.isEmpty()) {
            appendAppointmentRow(m_pendingAppointment);
            QMessageBox::information(this, "预约成功", "支付成功，患者已进入候诊。");
        }
        requestSchedules();
        requestOrderHistory();
        m_pendingAppointment.clear();
        m_pendingBillNo.clear();
        m_pendingPaymentToken.clear();
        m_pendingBillAmount = 0.0;
        m_pendingRegistrationInsuranceApproved = false;
    });
    connect(&dialog, &PaymentSelectionDialog::paymentPending, this, [this, &handled](const QString&) {
        handled = true;
        QMessageBox::information(this, "医保支付已提交", "医保页面已返回，系统将刷新订单状态。");
        requestOrderHistory();
        m_pendingAppointment.clear();
        m_pendingBillNo.clear();
        m_pendingPaymentToken.clear();
        m_pendingBillAmount = 0.0;
        m_pendingRegistrationInsuranceApproved = false;
    });
    dialog.exec();
    if (!handled && !m_pendingAppointment.isEmpty()) {
        m_pendingAppointment.clear();
        m_pendingBillNo.clear();
        m_pendingPaymentToken.clear();
        m_pendingBillAmount = 0.0;
        m_pendingRegistrationInsuranceApproved = false;
    }
}

void PatientAppointmentWindow::populateHistoryRows(const QJsonArray& rows)
{
    m_resultTable->setRowCount(0);
    for (const auto& item : rows) {
        const auto rowObject = item.toObject();
        const int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        const QStringList values = {
            rowObject.value("registrationNo").toString(),
            rowObject.value("patientName").toString(),
            rowObject.value("department").toString(),
            rowObject.value("doctorName").toString(),
            rowObject.value("visitDate").toString(),
            rowObject.value("timeSlot").toString(),
            rowObject.value("status").toString(),
            rowObject.value("billNo").toString()
        };
        for (int column = 0; column < values.size(); ++column) {
            auto* cell = new QTableWidgetItem(values.at(column));
            cell->setToolTip(values.at(column));
            m_resultTable->setItem(row, column, cell);
        }

        const QString status = rowObject.value("status").toString();
        const QString paymentStatus = rowObject.value("paymentStatus").toString();
        if (status == "待支付" || status == kPendingPaymentStatus || paymentStatus == "PENDING" || paymentStatus == "UNPAID") {
            auto* payButton = new QPushButton("去支付", m_resultTable);
            payButton->setProperty("billNo", rowObject.value("billNo").toString());
            payButton->setProperty("totalAmount", rowObject.value("totalAmount").toVariant());
            payButton->setProperty("registrationInsuranceApproved",
                                   rowObject.value("paymentIdentity").toString() == "医保统筹资格已通过"
                                       || rowObject.value("insuranceResultCode").toString() == "REG_INS_000");
            connect(payButton, &QPushButton::clicked, this, [this, payButton]() {
                startPaymentForBill(payButton->property("billNo").toString(),
                                    payButton->property("totalAmount").toDouble(),
                                    payButton->property("registrationInsuranceApproved").toBool());
            });
            m_resultTable->setCellWidget(row, 8, payButton);
        } else {
            m_resultTable->setItem(row, 8, new QTableWidgetItem("已处理"));
        }
    }
    m_resultTable->resizeColumnsToContents();
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
}

void PatientAppointmentWindow::startPaymentForBill(const QString& billNo, double totalAmount, bool registrationInsuranceApproved)
{
    if (!ensurePatientLoggedIn()) {
        return;
    }
    m_pendingRegistrationInsuranceApproved = registrationInsuranceApproved;
    showPaymentSelectionDialog(billNo, totalAmount);
}

void PatientAppointmentWindow::appendAppointmentRow(const QStringList& values)
{
    if (values.isEmpty()) {
        return;
    }

    const int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    for (int i = 0; i < values.size(); ++i) {
        auto* item = new QTableWidgetItem(values.at(i));
        item->setToolTip(values.at(i));
        m_resultTable->setItem(row, i, item);
    }
    m_resultTable->resizeColumnsToContents();
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
}

void PatientAppointmentWindow::chooseFirstAvailableSchedule()
{
    if (m_schedules.isEmpty()) {
        return;
    }

    QJsonObject bestSchedule;
    QDate bestDate;
    for (const auto& item : m_schedules) {
        const auto schedule = item.toObject();
        const QDate date = QDate::fromString(schedule.value("出诊日期").toString(), "yyyy-MM-dd");
        if (!date.isValid() || scheduleRemain(schedule) <= 0 || firstUsableTimeSlot(date).isEmpty()) {
            continue;
        }

        if (bestSchedule.isEmpty() || date < bestDate) {
            bestSchedule = schedule;
            bestDate = date;
        }
    }

    if (!bestSchedule.isEmpty()) {
        applyScheduleSelection(bestSchedule);
    }
}

bool PatientAppointmentWindow::chooseFirstAvailableScheduleForDepartment(const QString& department)
{
    if (department.startsWith("暂无") || department.trimmed().isEmpty()) {
        return false;
    }

    QJsonObject bestSchedule;
    QDate bestDate;
    for (const auto& item : m_schedules) {
        const auto schedule = item.toObject();
        const QDate date = QDate::fromString(schedule.value("出诊日期").toString(), "yyyy-MM-dd");
        if (!scheduleMatchesDepartment(schedule, department)
            || !date.isValid()
            || scheduleRemain(schedule) <= 0) {
            continue;
        }
        if (firstUsableTimeSlot(date).isEmpty()) {
            continue;
        }

        if (bestSchedule.isEmpty() || date < bestDate) {
            bestSchedule = schedule;
            bestDate = date;
        }
    }
    return applyScheduleSelection(bestSchedule);
}

bool PatientAppointmentWindow::applyScheduleSelection(const QJsonObject& schedule)
{
    if (schedule.isEmpty()) {
        return false;
    }

    const QString department = schedule.value("科室").toString().trimmed();
    const QDate date = QDate::fromString(schedule.value("出诊日期").toString(), "yyyy-MM-dd");
    const QString firstSlot = firstUsableTimeSlot(date);
    if (department.isEmpty() || !date.isValid() || firstSlot.isEmpty() || scheduleRemain(schedule) <= 0) {
        return false;
    }

    const QString category = DepartmentCatalog::categoryFor(department);
    const QString specialty = DepartmentCatalog::specialtyFor(department);
    const QString clinic = DepartmentCatalog::clinicFor(department);

    m_autoSelectingSchedule = true;
    if (!category.isEmpty()) {
        m_categoryBox->setCurrentText(category);
        updateSpecialties();
    }
    if (!specialty.isEmpty()) {
        m_specialtyBox->setCurrentText(specialty);
        updateClinics();
    }
    if (!clinic.isEmpty()) {
        const int index = m_departmentBox->findText(clinic);
        m_departmentBox->setCurrentIndex(index >= 0 ? index : 0);
    }
    m_dateEdit->setDate(date);
    m_autoSelectingSchedule = false;

    updateTimeSlots();
    const int slotIndex = m_timeSlotBox->findText(firstSlot);
    if (slotIndex >= 0) {
        m_timeSlotBox->setCurrentIndex(slotIndex);
    }
    updateDoctors();
    return true;
}

bool PatientAppointmentWindow::scheduleMatchesDepartment(const QJsonObject& schedule, const QString& department) const
{
    const QString selected = department.trimmed();
    const QString scheduleDepartment = schedule.value("科室").toString().trimmed();
    if (selected.isEmpty() || scheduleDepartment.isEmpty()) {
        return false;
    }
    if (scheduleDepartment == selected) {
        return true;
    }

    const QString selectedSpecialty = DepartmentCatalog::specialtyFor(selected);
    const QString scheduleSpecialty = DepartmentCatalog::specialtyFor(scheduleDepartment);
    return (!selectedSpecialty.isEmpty() && scheduleDepartment == selectedSpecialty)
        || (!scheduleSpecialty.isEmpty() && scheduleSpecialty == selectedSpecialty);
}

QString PatientAppointmentWindow::firstUsableTimeSlot(const QDate& date) const
{
    if (!date.isValid() || date < QDate::currentDate()) {
        return {};
    }

    const QStringList timeSlots = {
        "08:30-09:00", "09:00-09:30", "09:30-10:00", "10:00-10:30", "10:30-11:00",
        "13:30-14:00", "14:00-14:30", "14:30-15:00", "15:00-15:30", "15:30-16:00",
        "16:00-16:30", "16:30-17:00", "17:00-17:30", "17:30-18:00"
    };
    const bool isToday = date == QDate::currentDate();
    const QTime now = QTime::currentTime();
    for (const QString& slot : timeSlots) {
        const QTime end = QTime::fromString(slot.section('-', 1, 1), "HH:mm");
        if (isToday && end.isValid() && end <= now) {
            continue;
        }
        return slot;
    }
    return {};
}

void PatientAppointmentWindow::selectDepartmentPath(const QString& department)
{
    const QString category = DepartmentCatalog::categoryFor(department);
    const QString specialty = DepartmentCatalog::specialtyFor(department);
    const QString clinic = DepartmentCatalog::clinicFor(department);

    m_autoSelectingSchedule = true;
    if (!category.isEmpty()) {
        m_categoryBox->setCurrentText(category);
        updateSpecialties();
    }
    if (!specialty.isEmpty()) {
        m_specialtyBox->setCurrentText(specialty);
        updateClinics();
    }
    if (!clinic.isEmpty()) {
        m_departmentBox->setCurrentText(clinic);
    }
    m_autoSelectingSchedule = false;
    updateDoctors();
}

QString PatientAppointmentWindow::selectedClinic() const
{
    const QString clinic = m_departmentBox->currentText().trimmed();
    if (!clinic.isEmpty() && clinic != "全部诊室") {
        return clinic;
    }
    const QString specialty = m_specialtyBox->currentText().trimmed();
    if (!specialty.isEmpty()) {
        return specialty;
    }
    return DepartmentCatalog::firstClinic(m_categoryBox->currentText(), m_specialtyBox->currentText());
}

QString PatientAppointmentWindow::selectedAppointmentDepartment() const
{
    const QString scheduledDepartment = m_doctorBox->currentData().toString().trimmed();
    if (!scheduledDepartment.isEmpty()) {
        return scheduledDepartment;
    }
    return selectedClinic();
}

QString PatientAppointmentWindow::doctorNameFromDisplay(const QString& displayText) const
{
    return displayText.section(' ', 0, 0).section("（", 0, 0);
}

int PatientAppointmentWindow::scheduleRemain(const QJsonObject& schedule) const
{
    return schedule.value("剩余号源").toVariant().toInt();
}

} // namespace hospital::client
