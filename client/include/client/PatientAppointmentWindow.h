#pragma once

#include "common/Protocol.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QStringList>

class QComboBox;
class QCheckBox;
class QDateEdit;
class QLineEdit;
class QTableWidget;
class QTextEdit;
class QLabel;
class QPushButton;

namespace hospital { namespace client {

class ApiClient;
class PatientManager;

class PatientAppointmentWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PatientAppointmentWindow(ApiClient* apiClient,
                                      PatientManager* patientManager = nullptr,
                                      QWidget* parent = nullptr);

private slots:
    void requestSchedules();
    void updateDepartments();
    void updateSpecialties();
    void updateClinics();
    void updateDoctors();
    void updateTimeSlots();
    void requestInsurancePrecheck();
    void requestInsuranceProfile();
    void recommendDepartment();
    void addAppointment();
    void requestOrderHistory();
    void openUserCenter();
    void showPatientSwitcher();
    void addNewPatient();
    void onResponseReceived(const common::Response& response);

private:
    void appendAppointmentRow(const QStringList& values);
    void chooseFirstAvailableSchedule();
    bool chooseFirstAvailableScheduleForDepartment(const QString& department);
    bool applyScheduleSelection(const QJsonObject& schedule);
    bool scheduleMatchesDepartment(const QJsonObject& schedule, const QString& department) const;
    QString firstUsableTimeSlot(const QDate& date) const;
    void selectDepartmentPath(const QString& department);
    QString selectedClinic() const;
    QString selectedAppointmentDepartment() const;
    QString doctorNameFromDisplay(const QString& displayText) const;
    int scheduleRemain(const QJsonObject& schedule) const;
    bool ensurePatientLoggedIn();
    void applyCurrentPatient();
    void refreshCurrentPatientCard();
    void resetInsuranceCheck(bool switchToSelfPay = false);
    void setInsuranceChecking(bool checking);
    void openInsuranceProfileDialog(const QJsonObject& profile);
    void saveInsuranceProfile(const QJsonObject& payload);
    void showPaymentSelectionDialog(const QString& billNo, double totalAmount);
    void populateHistoryRows(const QJsonArray& rows);
    void startPaymentForBill(const QString& billNo, double totalAmount, bool registrationInsuranceApproved = false);
    bool m_autoSelectingSchedule = false;

    ApiClient* m_apiClient = nullptr;
    PatientManager* m_patientManager = nullptr;
    QJsonArray m_schedules;
    QPushButton* m_currentPatientCard = nullptr;
    QLabel* m_currentPatientTitle = nullptr;
    QLabel* m_currentPatientDetail = nullptr;
    QComboBox* m_categoryBox = nullptr;
    QComboBox* m_specialtyBox = nullptr;
    QComboBox* m_departmentBox = nullptr;
    QComboBox* m_doctorBox = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QComboBox* m_timeSlotBox = nullptr;
    QComboBox* m_paymentModeBox = nullptr;
    QCheckBox* m_emergencyCheckBox = nullptr;
    QPushButton* m_insuranceProfileButton = nullptr;
    QLineEdit* m_patientNameEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QLineEdit* m_idCardEdit = nullptr;
    QLineEdit* m_emergencyReasonEdit = nullptr;
    QTextEdit* m_symptomEdit = nullptr;
    QLabel* m_insuranceStatusLabel = nullptr;
    QPushButton* m_addAppointmentButton = nullptr;
    QTableWidget* m_resultTable = nullptr;
    QStringList m_pendingAppointment;
    QString m_pendingBillNo;
    QString m_pendingPaymentToken;
    QString m_insuranceToken;
    QString m_insuranceDataVersion;
    QString m_insuranceResultCode;
    double m_pendingBillAmount = 0.0;
    bool m_pendingRegistrationInsuranceApproved = false;
    bool m_isInsuranceChecking = false;
};

}} // namespace hospital::client
