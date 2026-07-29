#pragma once

#include "common/Protocol.h"

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

namespace hospital { namespace client {

class ApiClient;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(ApiClient* apiClient, QWidget* parent = nullptr);
    void setPresetUsername(const QString& username);

private slots:
    void login();
    void onConnected();
    void onErrorOccurred(const QString& message);
    void onResponseReceived(const common::Response& response);

private:
    ApiClient* m_apiClient = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_loginButton = nullptr;
    bool m_waitingForLogin = false;
};

}} // namespace hospital::client
