#pragma once

#include "common/Protocol.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace hospital { namespace client {

class ApiClient;

class PatientLoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatientLoginDialog(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void login();
    void registerPatient();
    void onConnected();
    void onErrorOccurred(const QString& message);
    void onResponseReceived(const common::Response& response);

private:
    ApiClient* m_apiClient = nullptr;
    QLineEdit* m_loginUsernameEdit = nullptr;
    QLineEdit* m_loginPasswordEdit = nullptr;
    QLineEdit* m_registerNameEdit = nullptr;
    QLineEdit* m_registerPhoneEdit = nullptr;
    QLineEdit* m_registerIdCardEdit = nullptr;
    QLineEdit* m_registerPasswordEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_loginButton = nullptr;
    QPushButton* m_registerButton = nullptr;
    QString m_pendingAction;
};

}} // namespace hospital::client
