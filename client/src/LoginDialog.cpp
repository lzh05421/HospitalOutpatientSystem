#include "client/LoginDialog.h"

#include "client/ApiClient.h"

#include <QFrame>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace hospital::client {

LoginDialog::LoginDialog(ApiClient* apiClient, QWidget* parent)
    : QDialog(parent)
    , m_apiClient(apiClient)
{
    setWindowTitle("医院门诊系统登录");
    resize(500, 340);
    setObjectName("loginDialog");

    m_usernameEdit = new QLineEdit(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_usernameEdit->setText("admin");
    m_passwordEdit->setText("123456");
    m_usernameEdit->setMinimumHeight(38);
    m_passwordEdit->setMinimumHeight(38);

    auto* loginHero = new QFrame(this);
    loginHero->setObjectName("loginHero");
    auto* heroLayout = new QVBoxLayout(loginHero);
    heroLayout->setContentsMargins(18, 18, 18, 18);
    heroLayout->setSpacing(8);

    auto* statusPill = new QLabel("院内账号认证", loginHero);
    statusPill->setObjectName("statusPill");
    statusPill->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("医院人员登录", loginHero);
    title->setObjectName("dialogTitle");
    title->setAlignment(Qt::AlignCenter);

    auto* subtitle = new QLabel("默认账号：admin  默认密码：123456", loginHero);
    subtitle->setObjectName("mutedText");
    subtitle->setAlignment(Qt::AlignCenter);

    auto* secureTip = new QLabel("登录后进入统一医疗工作站，按角色接入门诊、收费、药房和驾驶舱模块。", loginHero);
    secureTip->setObjectName("secureTip");
    secureTip->setAlignment(Qt::AlignCenter);
    secureTip->setWordWrap(true);

    heroLayout->addWidget(statusPill, 0, Qt::AlignHCenter);
    heroLayout->addWidget(title);
    heroLayout->addWidget(subtitle);
    heroLayout->addWidget(secureTip);

    auto* loginCard = new QFrame(this);
    loginCard->setObjectName("loginCard");
    auto* loginCardLayout = new QVBoxLayout(loginCard);
    loginCardLayout->setContentsMargins(18, 18, 18, 18);
    loginCardLayout->setSpacing(12);

    auto* formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(14);
    formLayout->addRow("用户名", m_usernameEdit);
    formLayout->addRow("密码", m_passwordEdit);

    m_statusLabel = new QLabel(loginCard);
    m_statusLabel->setObjectName("statusText");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText(m_apiClient->isConnected() ? "服务端已连接" : "正在连接服务端 127.0.0.1:8899...");

    m_loginButton = new QPushButton("登录后台", loginCard);
    m_loginButton->setMinimumHeight(42);
    m_loginButton->setObjectName("primaryButton");

    loginCardLayout->addLayout(formLayout);
    loginCardLayout->addWidget(m_statusLabel);
    loginCardLayout->addWidget(m_loginButton);

    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::login);
    connect(m_apiClient, &ApiClient::connected, this, &LoginDialog::onConnected);
    connect(m_apiClient, &ApiClient::errorOccurred, this, &LoginDialog::onErrorOccurred);
    connect(m_apiClient, &ApiClient::responseReceived, this, &LoginDialog::onResponseReceived);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(14);
    rootLayout->addWidget(loginHero);
    rootLayout->addWidget(loginCard);

    setStyleSheet(R"(
        QDialog#loginDialog {
            background: #eef4f6;
        }
        QFrame#loginHero {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fbfb, stop:1 #ecf7f5);
            border: 1px solid #d4e5e1;
            border-radius: 16px;
        }
        QFrame#loginCard {
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
        QLabel#mutedText, QLabel#statusText {
            color: #667085;
            font-size: 13px;
        }
        QLabel#secureTip {
            color: #5f7080;
            font-size: 13px;
        }
        QLineEdit {
            border: 1px solid #c9d8d7;
            border-radius: 10px;
            padding: 8px 10px;
            background: #ffffff;
            font-size: 15px;
        }
        QLineEdit:focus {
            border-color: #0f766e;
            background: #fbfefd;
        }
        QPushButton#primaryButton {
            border: none;
            border-radius: 10px;
            background: #0f766e;
            color: white;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background: #0d9488;
        }
    )");
}

void LoginDialog::setPresetUsername(const QString& username)
{
    if (username.trimmed().isEmpty()) {
        return;
    }
    m_usernameEdit->setText(username.trimmed());
    m_passwordEdit->setText("123456");
}

void LoginDialog::login()
{
    if (!m_apiClient->isConnected()) {
        m_statusLabel->setText("服务端未连接，正在重试连接...");
        m_apiClient->connectToServer("127.0.0.1", 8899);
        QTimer::singleShot(800, this, [this]() {
            if (!m_apiClient->isConnected()) {
                QMessageBox::warning(this, "无法登录", "服务端未连接。请先运行 hospital_server，或直接双击“运行项目.bat”。");
            }
        });
        return;
    }

    m_waitingForLogin = true;
    m_loginButton->setEnabled(false);
    m_statusLabel->setText("正在验证账号...");

    common::Request request;
    request.module = "auth";
    request.action = "login";
    request.payload["username"] = m_usernameEdit->text();
    request.payload["password"] = m_passwordEdit->text();
    if (!m_apiClient->send(request)) {
        m_waitingForLogin = false;
        m_loginButton->setEnabled(true);
    }
}

void LoginDialog::onConnected()
{
    m_statusLabel->setText("服务端已连接");
}

void LoginDialog::onErrorOccurred(const QString& message)
{
    if (m_statusLabel) {
        m_statusLabel->setText("连接失败：" + message);
    }
    if (m_loginButton) {
        m_loginButton->setEnabled(true);
    }
}

void LoginDialog::onResponseReceived(const common::Response& response)
{
    if (!m_waitingForLogin
        || response.data.value("module").toString() != "auth"
        || response.data.value("action").toString() != "login") {
        return;
    }

    m_waitingForLogin = false;
    m_loginButton->setEnabled(true);

    if (!response.success) {
        QMessageBox::warning(this, "登录失败", response.message);
        return;
    }

    m_apiClient->setSession(response.data);
    accept();
}

} // namespace hospital::client
