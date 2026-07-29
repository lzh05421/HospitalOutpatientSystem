#pragma once

#include "common/Protocol.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace hospital { namespace client {

class ApiClient;

struct PatientProfile
{
    QString patientId;
    QString name;
    QString phone;
    QString idCard;
    QString gender;
    QString relationship;
};

class PatientManager : public QObject
{
    Q_OBJECT

public:
    explicit PatientManager(ApiClient* apiClient, QObject* parent = nullptr);

    bool hasCurrentPatient() const;
    PatientProfile currentPatient() const;
    QList<PatientProfile> patients() const;

    void loadPatients();
    void addPatient(const QString& name,
                    const QString& phone,
                    const QString& idCard,
                    const QString& gender,
                    const QString& relationship);
    bool selectPatient(const QString& patientId);
    void loadMyHistory(bool onlyCurrentPatient = true);

signals:
    void patientsLoaded();
    void currentPatientChanged();
    void historyLoaded(const QJsonArray& rows);
    void errorOccurred(const QString& message);

private slots:
    void onResponseReceived(const common::Response& response);

private:
    PatientProfile profileFromJson(const QJsonObject& object) const;
    void setPatientsFromJson(const QJsonArray& rows);
    void selectInitialPatient();

    ApiClient* m_apiClient = nullptr;
    QList<PatientProfile> m_patients;
    PatientProfile m_currentPatient;
    QString m_pendingAction;
};

}} // namespace hospital::client
