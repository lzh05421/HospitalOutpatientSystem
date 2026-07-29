#pragma once

#include <QJsonArray>
#include <QMainWindow>

class QLabel;
class QTableWidget;
class QTabWidget;

namespace hospital { namespace client {

class ApiClient;
class PatientManager;

class UserCenterWindow : public QMainWindow
{
    Q_OBJECT

public:
    UserCenterWindow(ApiClient* apiClient, PatientManager* patientManager, QWidget* parent = nullptr);

private slots:
    void refreshHeader();
    void showPatientSwitcher();
    void populateHistory(const QJsonArray& rows);

private:
    void requestHistory();
    QTableWidget* createPlaceholderTable(const QString& text);

    ApiClient* m_apiClient = nullptr;
    PatientManager* m_patientManager = nullptr;
    QLabel* m_accountLabel = nullptr;
    QLabel* m_patientLabel = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTableWidget* m_historyTable = nullptr;
};

}} // namespace hospital::client
