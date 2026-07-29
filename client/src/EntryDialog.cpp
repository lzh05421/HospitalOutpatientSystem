#include "client/EntryDialog.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace hospital::client {

EntryDialog::EntryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("医院门诊系统");
    resize(620, 430);
    setObjectName("entryDialog");

    auto* heroPanel = new QFrame(this);
    heroPanel->setObjectName("heroPanel");
    auto* heroLayout = new QVBoxLayout(heroPanel);
    heroLayout->setContentsMargins(22, 22, 22, 22);
    heroLayout->setSpacing(8);

    auto* statusPill = new QLabel("医疗工作站", heroPanel);
    statusPill->setObjectName("statusPill");
    statusPill->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("医院门诊挂号与药品管理系统", heroPanel);
    title->setObjectName("dialogTitle");
    QFont font = title->font();
    font.setPointSize(20);
    font.setBold(true);
    title->setFont(font);
    title->setAlignment(Qt::AlignCenter);

    auto* subtitle = new QLabel("统一接诊、候诊、收费、药房与检查流程的门诊业务工作站。", heroPanel);
    subtitle->setObjectName("heroSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    auto* entryAccent = new QLabel("选择入口", heroPanel);
    entryAccent->setObjectName("entryAccent");
    entryAccent->setAlignment(Qt::AlignCenter);

    heroLayout->addWidget(statusPill, 0, Qt::AlignHCenter);
    heroLayout->addWidget(title);
    heroLayout->addWidget(subtitle);
    heroLayout->addWidget(entryAccent);

    auto* patientButton = new QPushButton("患者登录/注册", this);
    auto* staffButton = new QPushButton("医院人员登录", this);
    patientButton->setObjectName("primaryButton");
    staffButton->setObjectName("secondaryButton");
    patientButton->setMinimumHeight(44);
    staffButton->setMinimumHeight(44);

    auto* rolePanel = new QFrame(this);
    rolePanel->setObjectName("rolePanel");
    auto* rolePanelLayout = new QVBoxLayout(rolePanel);
    rolePanelLayout->setContentsMargins(18, 16, 18, 16);
    rolePanelLayout->setSpacing(12);

    auto* roleLabel = new QLabel("选择医院人员角色", rolePanel);
    roleLabel->setObjectName("roleLabel");
    roleLabel->setAlignment(Qt::AlignCenter);

    auto* roleGroup = new QButtonGroup(this);
    auto* adminRadio = new QRadioButton("系统管理员", this);
    auto* directorRadio = new QRadioButton("科主任", this);
    auto* registrarRadio = new QRadioButton("挂号员", this);
    auto* doctorRadio = new QRadioButton("医生", this);
    auto* pharmacyRadio = new QRadioButton("药房人员", this);
    auto* cashierRadio = new QRadioButton("收费员", this);
    adminRadio->setChecked(true);
    roleGroup->addButton(adminRadio);
    roleGroup->addButton(directorRadio);
    roleGroup->addButton(registrarRadio);
    roleGroup->addButton(doctorRadio);
    roleGroup->addButton(pharmacyRadio);
    roleGroup->addButton(cashierRadio);

    auto* roleLayout = new QGridLayout();
    roleLayout->setHorizontalSpacing(18);
    roleLayout->setVerticalSpacing(10);
    roleLayout->addWidget(adminRadio, 0, 0);
    roleLayout->addWidget(directorRadio, 0, 1);
    roleLayout->addWidget(registrarRadio, 0, 2);
    roleLayout->addWidget(doctorRadio, 1, 0);
    roleLayout->addWidget(pharmacyRadio, 1, 1);
    roleLayout->addWidget(cashierRadio, 1, 2);

    auto* hint = new QLabel("患者登录后可自动填充就诊人信息并查看自己的历史挂号记录；医院人员按角色登录后台。", rolePanel);
    hint->setObjectName("hintText");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);

    rolePanelLayout->addWidget(roleLabel);
    rolePanelLayout->addLayout(roleLayout);
    rolePanelLayout->addWidget(hint);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(heroPanel);
    layout->addSpacing(6);
    layout->addWidget(patientButton);
    layout->addWidget(staffButton);
    layout->addSpacing(8);
    layout->addWidget(rolePanel);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    setStyleSheet(R"(
        QDialog#entryDialog {
            background: #eef4f6;
        }
        QLabel {
            color: #18212f;
        }
        QFrame#heroPanel {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fbfb, stop:1 #ecf7f5);
            border: 1px solid #d4e5e1;
            border-radius: 16px;
        }
        QFrame#rolePanel {
            background: #ffffff;
            border: 1px solid #d7e4e2;
            border-radius: 14px;
        }
        QLabel#statusPill {
            min-width: 96px;
            padding: 6px 14px;
            border-radius: 999px;
            background: #dff7f3;
            color: #0f766e;
            border: 1px solid #99f6e4;
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#dialogTitle {
            color: #0f172a;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#heroSubtitle {
            color: #5f7080;
            font-size: 13px;
        }
        QLabel#entryAccent {
            color: #0f766e;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#roleLabel {
            color: #4b5563;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#hintText {
            color: #667085;
            font-size: 13px;
        }
        QRadioButton {
            color: #1f2937;
            spacing: 8px;
            font-size: 13px;
        }
        QRadioButton::indicator {
            width: 18px;
            height: 18px;
        }
        QRadioButton::indicator:unchecked {
            border: 1px solid #94a3b8;
            border-radius: 9px;
            background: #ffffff;
        }
        QRadioButton::indicator:checked {
            border: 1px solid #0f766e;
            border-radius: 9px;
            background: #dff7f3;
        }
        QPushButton {
            min-height: 46px;
            border-radius: 12px;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#primaryButton {
            background: #0f766e;
            color: white;
            border: none;
        }
        QPushButton#secondaryButton {
            background: #ffffff;
            color: #0f766e;
            border: 1px solid #c7ddd8;
        }
        QPushButton#primaryButton:hover {
            background: #0d9488;
        }
        QPushButton#secondaryButton:hover {
            border-color: #0f766e;
            background: #f7fbfb;
        }
    )");

    connect(patientButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::PatientLogin;
        accept();
    });
    connect(staffButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::StaffLogin;
        accept();
    });
    connect(adminRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "admin";
    });
    connect(directorRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "director01";
    });
    connect(registrarRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "reg01";
    });
    connect(doctorRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "doctor01";
    });
    connect(pharmacyRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "pharmacy01";
    });
    connect(cashierRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_staffUsername = "cashier01";
    });
}

EntryDialog::Choice EntryDialog::choice() const
{
    return m_choice;
}

QString EntryDialog::staffUsername() const
{
    return m_staffUsername;
}

} // namespace hospital::client
