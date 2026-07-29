#include "client/PatientLoginDialog.h"

#include "client/ApiClient.h"

#include <QFrame>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace hospital::client {

PatientLoginDialog::PatientLoginDialog(ApiClient* apiClient, QWidget* parent)
    : QDialog(parent)
    , m_apiClient(apiClient)
{
    setWindowTitle("患者登录/注册");
    resize(520, 420);
    setObjectName("patientLoginDialog");

    auto* patientLoginHero = new QFrame(this);
    patientLoginHero->setObjectName("patientLoginHero");
    auto* heroLayout = new QVBoxLayout(patientLoginHero);
    heroLayout->setContentsMargins(18, 18, 18, 18);
    heroLayout->setSpacing(8);

    auto* statusPill = new QLabel("门诊预约入口", patientLoginHero);
    statusPill->setObjectName("statusPill");
    statusPill->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("患者登录/注册", patientLoginHero);
    title->setObjectName("dialogTitle");
    title->setAlignment(Qt::AlignCenter);

    auto* subtitle = new QLabel("登录后可预约挂号、查看历史订单与个人就诊资料。", patientLoginHero);
    subtitle->setObjectName("patientHeroSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    heroLayout->addWidget(statusPill, 0, Qt::AlignHCenter);
    heroLayout->addWidget(title);
    heroLayout->addWidget(subtitle);

    auto* patientLoginCard = new QFrame(this);
    patientLoginCard->setObjectName("patientLoginCard");
    auto* patientCardLayout = new QVBoxLayout(patientLoginCard);
    patientCardLayout->setContentsMargins(16, 16, 16, 16);
    patientCardLayout->setSpacing(12);

    auto* tabs = new QTabWidget(patientLoginCard);

    auto* loginPage = new QWidget(tabs);
    auto* loginForm = new QFormLayout(loginPage);
    m_loginUsernameEdit = new QLineEdit("13800000001", loginPage);
    m_loginPasswordEdit = new QLineEdit("123456", loginPage);
    m_loginPasswordEdit->setEchoMode(QLineEdit::Password);
    m_loginButton = new QPushButton("登录并预约", loginPage);
    m_loginButton->setObjectName("primaryButton");
    loginForm->addRow("账号/手机号", m_loginUsernameEdit);
    loginForm->addRow("密码", m_loginPasswordEdit);
    loginForm->addRow("", m_loginButton);

    auto* registerPage = new QWidget(tabs);
    auto* registerForm = new QFormLayout(registerPage);
    m_registerNameEdit = new QLineEdit(registerPage);
    m_registerPhoneEdit = new QLineEdit(registerPage);
    m_registerIdCardEdit = new QLineEdit(registerPage);
    m_registerPasswordEdit = new QLineEdit(registerPage);
    m_registerPasswordEdit->setEchoMode(QLineEdit::Password);
    m_registerButton = new QPushButton("注册并登录", registerPage);
    m_registerButton->setObjectName("primaryButton");
    registerForm->addRow("姓名", m_registerNameEdit);
    registerForm->addRow("手机号", m_registerPhoneEdit);
    registerForm->addRow("身份证号", m_registerIdCardEdit);
    registerForm->addRow("密码", m_registerPasswordEdit);
    registerForm->addRow("", m_registerButton);

    tabs->addTab(loginPage, "账号密码登录");
    tabs->addTab(registerPage, "新用户注册");

    m_statusLabel = new QLabel(m_apiClient->isConnected() ? "服务端已连接" : "正在连接服务端 127.0.0.1:8899...", patientLoginCard);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setObjectName("statusText");
    patientCardLayout->addWidget(tabs, 1);
    patientCardLayout->addWidget(m_statusLabel);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(14);
    root->addWidget(patientLoginHero);
    root->addWidget(patientLoginCard, 1);

    connect(m_loginButton, &QPushButton::clicked, this, &PatientLoginDialog::login);
    connect(m_registerButton, &QPushButton::clicked, this, &PatientLoginDialog::registerPatient);
    connect(m_apiClient, &ApiClient::connected, this, &PatientLoginDialog::onConnected);
    connect(m_apiClient, &ApiClient::errorOccurred, this, &PatientLoginDialog::onErrorOccurred);
    connect(m_apiClient, &ApiClient::responseReceived, this, &PatientLoginDialog::onResponseReceived);

    setStyleSheet(R"(
        QDialog#patientLoginDialog {
            background: #eef4f6;
        }
        QFrame#patientLoginHero {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fbfb, stop:1 #ecf7f5);
            border: 1px solid #d4e5e1;
            border-radius: 16px;
        }
        QFrame#patientLoginCard {
            background: #ffffff;
            border: 1px solid #d7e4e2;
            border-radius: 16px;
        }
        QLabel#statusPill {
            min-width: 108px;
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
        QLabel#patientHeroSubtitle, QLabel#statusText {
            color: #667085;
            font-size: 13px;
        }
        QLabel#statusText {
            padding-top: 6px;
        }
        QLineEdit {
            border: 1px solid #c9d8d7;
            border-radius: 10px;
            padding: 7px 10px;
            min-height: 34px;
            background: white;
        }
        QLineEdit:focus {
            border-color: #0f766e;
            background: #fbfefd;
        }
        QTabWidget::pane {
            border: none;
            background: transparent;
        }
        QTabBar::tab {
            min-width: 118px;
            padding: 8px 14px;
            margin-right: 6px;
            border-radius: 10px;
            background: #eef4f4;
            color: #5f7080;
            font-weight: 600;
        }
        QTabBar::tab:selected {
            background: #dff7f3;
            color: #0f766e;
        }
        QPushButton#primaryButton {
            min-height: 40px;
            border-radius: 10px;
            border: none;
            background: #0f766e;
            color: white;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background: #0d9488;
        }
    )");
}

void PatientLoginDialog::login()
{
    if (!m_apiClient->isConnected()) {
        m_statusLabel->setText("服务端未连接，正在重试连接...");
        m_apiClient->connectToServer("127.0.0.1", 8899);
        QTimer::singleShot(800, this, [this]() {
            if (!m_apiClient->isConnected()) {
                QMessageBox::warning(this, "无法登录", "服务端未连接，请先启动 hospital_server。");
            }
        });
        return;
    }

    m_pendingAction = "patientLogin";
    m_loginButton->setEnabled(false);
    m_statusLabel->setText("正在验证患者账号...");
    common::Request request;
    request.module = "auth";
    request.action = "patientLogin";
    request.payload["username"] = m_loginUsernameEdit->text().trimmed();
    request.payload["password"] = m_loginPasswordEdit->text();
    if (!m_apiClient->send(request)) {
        m_pendingAction.clear();
        m_loginButton->setEnabled(true);
    }
}

void PatientLoginDialog::registerPatient()
{
    const QString idCard = m_registerIdCardEdit->text().trimmed();
    if (!QRegularExpression(QStringLiteral("^\\d{17}[0-9Xx]$")).match(idCard).hasMatch()) {
        QMessageBox::warning(this, "注册失败", "身份证号格式不正确。");
        return;
    }

    m_pendingAction = "patientRegister";
    m_registerButton->setEnabled(false);
    m_statusLabel->setText("正在创建患者账号...");
    common::Request request;
    request.module = "auth";
    request.action = "patientRegister";
    request.payload["name"] = m_registerNameEdit->text().trimmed();
    request.payload["phone"] = m_registerPhoneEdit->text().trimmed();
    request.payload["username"] = m_registerPhoneEdit->text().trimmed();
    request.payload["idCard"] = idCard;
    request.payload["password"] = m_registerPasswordEdit->text();
    if (!m_apiClient->send(request)) {
        m_pendingAction.clear();
        m_registerButton->setEnabled(true);
    }
}

void PatientLoginDialog::onConnected()
{
    m_statusLabel->setText("服务端已连接");
}

void PatientLoginDialog::onErrorOccurred(const QString& message)
{
    m_statusLabel->setText("连接失败：" + message);
    m_loginButton->setEnabled(true);
    m_registerButton->setEnabled(true);
}

void PatientLoginDialog::onResponseReceived(const common::Response& response)
{
    if (m_pendingAction.isEmpty()
        || response.data.value("module").toString() != "auth"
        || response.data.value("action").toString() != m_pendingAction) {
        return;
    }

    const QString action = m_pendingAction;
    m_pendingAction.clear();
    m_loginButton->setEnabled(true);
    m_registerButton->setEnabled(true);

    if (!response.success) {
        QMessageBox::warning(this, action == "patientRegister" ? "注册失败" : "登录失败", response.message);
        return;
    }

    m_apiClient->setPatientSession(response.data);
    accept();
}

} // namespace hospital::client
