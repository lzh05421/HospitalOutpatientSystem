#pragma once

#include "client/pages/ModulePage.h"

class QPushButton;
class QComboBox;
class QDate;
class QDateEdit;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QListWidget;
class QListWidgetItem;

namespace hospital { namespace client {

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onResponseReceived(const common::Response& response);
    void exportDashboardCsv();

private:
    void updateWarningTable(const QJsonArray& rows);
    void updateDoctorTable(const QJsonArray& rows);

    ApiClient* m_apiClient = nullptr;
    QDateEdit* m_startDateEdit = nullptr;
    QDateEdit* m_endDateEdit = nullptr;
    QLabel* m_todayRegistrations = nullptr;
    QLabel* m_waitingPatients = nullptr;
    QLabel* m_finishedPatients = nullptr;
    QLabel* m_todayIncome = nullptr;
    QLabel* m_incomeMix = nullptr;
    QTableWidget* m_warningTable = nullptr;
    QTableWidget* m_doctorTable = nullptr;
    QWidget* m_dailyVisitsChart = nullptr;
    QWidget* m_departmentVisitsChart = nullptr;
    QWidget* m_doctorRankingChart = nullptr;
    QJsonArray m_dailyVisits;
    QJsonArray m_departmentVisits;
    QJsonArray m_doctorRanking;
    QJsonArray m_stockWarnings;
};

class PatientPage : public ModulePage
{
    Q_OBJECT
public:
    explicit PatientPage(ApiClient* apiClient, QWidget* parent = nullptr);
};

class PatientRecordPage : public ModulePage
{
    Q_OBJECT
public:
    explicit PatientRecordPage(ApiClient* apiClient, QWidget* parent = nullptr);
};

class RegistrationPage : public ModulePage
{
    Q_OBJECT
public:
    explicit RegistrationPage(ApiClient* apiClient, QWidget* parent = nullptr);
};

class WaitingQueuePage : public ModulePage
{
    Q_OBJECT
public:
    explicit WaitingQueuePage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private slots:
    void callSelectedPatient();
    void markSelectedEmergency();

private:
    QLabel* m_waitingCountLabel = nullptr;
    QLabel* m_calledCountLabel = nullptr;
    QLabel* m_emergencyCountLabel = nullptr;
    QLabel* m_averageWaitLabel = nullptr;
};

class SchedulePage : public ModulePage
{
    Q_OBJECT
public:
    explicit SchedulePage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private slots:
    void onDoctorListReceived(const common::Response& response);
    void updateScheduleSpecialtyOptions();
    void updateScheduleClinicOptions();
    void updateScheduleDoctorOptions();
    void smartSchedule();
    void resetSchedules();
    void showUnscheduledDoctors();

private:
    common::Response sendScheduleRequestSync(const QString& action,
                                             const QJsonObject& payload = QJsonObject()) const;
    QStringList loadServerScheduleRules(bool* ok = nullptr) const;
    bool saveServerScheduleRules(const QStringList& rules, QString* message = nullptr) const;
    QJsonArray loadServerScheduleRange(const QDate& startDate, const QDate& endDate, bool* ok = nullptr) const;
    bool hasSchedule(const QString& doctor, const QString& date) const;
    bool hasScheduleInRows(const QJsonArray& rows, const QString& doctor, const QString& date) const;
    bool hasActiveClinicCoverage(const QJsonArray& rows, const QString& clinic, const QString& date) const;
    QStringList doctorsForClinic(const QString& clinic) const;
    bool shouldDoctorWorkOnRotation(const QString& doctor, int dayOffset, const QStringList& clinicDoctors) const;
    int scheduledHalfDays(const QString& doctor, const QDate& startDate) const;
    int scheduledHalfDaysInRows(const QJsonArray& rows, const QString& doctor, const QDate& startDate) const;
    QString clinicForDoctor(const QJsonObject& doctor) const;
    int quotaForDoctor(const QJsonObject& doctor) const;

    QJsonArray m_scheduleRows;
    QJsonArray m_scheduleDoctors;
    QComboBox* m_scheduleCategoryBox = nullptr;
    QComboBox* m_scheduleSpecialtyBox = nullptr;
    QComboBox* m_scheduleDepartmentBox = nullptr;
    QComboBox* m_scheduleDoctorBox = nullptr;
};

class DepartmentPage : public ModulePage
{
    Q_OBJECT
public:
    explicit DepartmentPage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private slots:
    void addDepartment();
    void fillFormFromSelection();
    void updateSelectedDepartment();
    void deleteSelectedDepartment();

private:
    QLineEdit* m_codeEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_locationEdit = nullptr;
};

class ConsultationPage : public ModulePage
{
    Q_OBJECT
public:
    explicit ConsultationPage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private slots:
    void callSelectedPatient();
    void startConsultation();
    void onConsultationResponse(const common::Response& response);

private:
    void loadExaminationItems();
    QPushButton* m_callButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QLabel* m_pendingConsultationLabel = nullptr;
    QLabel* m_inConsultationLabel = nullptr;
    QLabel* m_reviewCountLabel = nullptr;
    QLabel* m_finishedTodayLabel = nullptr;
    QJsonArray m_examinationItems;
    QJsonObject m_pendingExamRequest;
    QJsonObject m_pendingPrescriptionRequest;
};

class ExaminationPage : public ModulePage
{
    Q_OBJECT
public:
    explicit ExaminationPage(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void onExaminationResponse(const common::Response& response);
    void loadExaminationItems();
    void saveExaminationItem();
    void deleteExaminationItem();
    void createExamination();
    void completeExamination();

private:
    QJsonArray m_examinationItems;
};

class PrescriptionPage : public ModulePage
{
    Q_OBJECT
public:
    explicit PrescriptionPage(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void createPrescription();
    void reviewPrescription();
    void dispensePrescription();
    void returnPrescription();
};

class DrugInventoryPage : public ModulePage
{
    Q_OBJECT
public:
    explicit DrugInventoryPage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private:
    QComboBox* m_categoryBox = nullptr;
};

class DoctorManagementPage : public ModulePage
{
    Q_OBJECT
public:
    explicit DoctorManagementPage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private slots:
    void updateSpecialtyOptions();
    void addDoctor();
    void fillFormFromSelection();
    void updateSelectedDoctor();
    void deleteSelectedDoctor();

private:
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_categoryBox = nullptr;
    QComboBox* m_departmentBox = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_specialtyEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QSpinBox* m_feeSpin = nullptr;
};

class BillingPage : public ModulePage
{
    Q_OBJECT
public:
    explicit BillingPage(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void paySelectedBill();
    void refundSelectedBill();
    void reviewSelectedRefund();
};

class StatisticsPage : public ModulePage
{
    Q_OBJECT
public:
    explicit StatisticsPage(ApiClient* apiClient, QWidget* parent = nullptr);

protected:
    void rowsUpdated(const QJsonArray& rows) override;

private:
    QWidget* m_chart = nullptr;
    QJsonArray m_rows;

private slots:
    void exportCsv();
};

class OperationLogPage : public ModulePage
{
    Q_OBJECT
public:
    explicit OperationLogPage(ApiClient* apiClient, QWidget* parent = nullptr);
};

class PermissionAdminPage : public QWidget
{
    Q_OBJECT
public:
    explicit PermissionAdminPage(ApiClient* apiClient, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onResponseReceived(const common::Response& response);
    void fillUserFormFromSelection();
    void loadSelectedRolePermissions();
    void updatePermissionItemVisual(QListWidgetItem* item);
    void createUser();
    void resetPassword();
    void toggleUser();
    void saveRolePermissions();

private:
    void requestUsers();
    void requestRoles(const QString& roleId = QString());
    void renderUsers(const QJsonArray& rows);
    void renderRoles(const QJsonArray& roles, const QJsonArray& permissions, const QString& selectedRoleId);
    QJsonObject selectedUser() const;
    QString selectedRoleId() const;
    QString selectedRoleCode() const;

    ApiClient* m_apiClient = nullptr;
    QTableWidget* m_userTable = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_realNameEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QComboBox* m_userRoleBox = nullptr;
    QPushButton* m_toggleUserButton = nullptr;
    QComboBox* m_roleBox = nullptr;
    QListWidget* m_permissionList = nullptr;
    QJsonArray m_roles;
};

}} // namespace hospital::client
