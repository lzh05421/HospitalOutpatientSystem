#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QString>

namespace hospital { namespace server {

class DemoRepository
{
public:
    static DemoRepository& instance();

    QJsonArray rows(const QString& key, const QString& keyword = QString());
    QJsonArray patientRecords(const QString& keyword = QString());
    QJsonArray waitingQueue(const QString& keyword = QString(),
                            const QString& departmentFilter = QString(),
                            const QString& doctorFilter = QString(),
                            const QString& clinicTypeFilter = QString());
    QJsonArray activeConsultations(const QString& keyword = QString(),
                                   const QString& departmentFilter = QString(),
                                   const QString& doctorFilter = QString(),
                                   const QString& clinicTypeFilter = QString());
    QJsonObject createExamination(const QJsonObject& payload);
    QJsonObject completeExamination(const QJsonObject& payload);
    QJsonObject updatePatientRecord(const QJsonObject& payload);
    QJsonObject deletePatientRecord(const QJsonObject& payload);
    QJsonObject saveDepartment(const QJsonObject& payload);
    QJsonObject updateDepartment(const QJsonObject& payload);
    QJsonObject disableDepartment(const QJsonObject& payload);
    QJsonObject saveDoctor(const QJsonObject& payload);
    QJsonObject updateDoctor(const QJsonObject& payload);
    QJsonObject disableDoctor(const QJsonObject& payload);
    QJsonObject addRegistration(const QJsonObject& payload);
    QJsonObject updateRegistration(const QJsonObject& payload);
    QJsonObject cancelRegistration(const QJsonObject& payload);
    QJsonObject callRegistration(const QJsonObject& payload);
    QJsonObject startConsultation(const QJsonObject& payload);
    QJsonObject saveConsultation(const QJsonObject& payload, bool backToWaiting = false);
    QJsonObject createPrescription(const QJsonObject& payload);
    QJsonObject reviewPrescription(const QJsonObject& payload);
    QJsonObject rejectPrescription(const QJsonObject& payload);
    QJsonObject dispensePrescription(const QJsonObject& payload);
    QJsonObject returnPrescription(const QJsonObject& payload);
    QJsonObject saveSchedule(const QJsonObject& payload);
    QJsonObject disableSchedule(const QJsonObject& payload);
    QJsonObject resetSchedules();
    QJsonObject updatePatient(const QJsonObject& payload);
    QJsonObject deletePatient(const QJsonObject& payload);
    QJsonObject addInventory(const QJsonObject& payload);
    QJsonObject updateInventory(const QJsonObject& payload);
    QJsonObject disableInventory(const QJsonObject& payload);
    QJsonObject updateBill(const QJsonObject& payload);
    QJsonObject cancelBill(const QJsonObject& payload);
    QJsonObject payBill(const QJsonObject& payload);
    QJsonObject refundBill(const QJsonObject& payload);
    void appendOperationLog(const QString& operatorName,
                            const QString& module,
                            const QString& action,
                            const QString& content);

private:
    DemoRepository();

    QJsonObject makeResult(bool success, const QString& message) const;
    int findScheduleIndex(const QString& doctor, const QString& date, const QString& period, const QString& department = QString()) const;
    void ensureCatalogData();
    QString stateFilePath() const;
    void loadState();
    void saveState() const;

    mutable QMutex m_mutex;
    QJsonArray m_patients;
    QJsonArray m_departments;
    QJsonArray m_registrations;
    QJsonArray m_schedules;
    QJsonArray m_doctors;
    QJsonArray m_consultations;
    QJsonArray m_prescriptions;
    QJsonArray m_examinations;
    QJsonArray m_inventory;
    QJsonArray m_bills;
    QJsonArray m_statistics;
    QJsonArray m_operationLogs;
};

}} // namespace hospital::server
