#include "client/PatientManager.h"

#include "client/ApiClient.h"

namespace hospital::client {

PatientManager::PatientManager(ApiClient* apiClient, QObject* parent)
    : QObject(parent)
    , m_apiClient(apiClient)
{
    connect(m_apiClient, &ApiClient::responseReceived, this, &PatientManager::onResponseReceived);
}

bool PatientManager::hasCurrentPatient() const
{
    return !m_currentPatient.patientId.trimmed().isEmpty();
}

PatientProfile PatientManager::currentPatient() const
{
    return m_currentPatient;
}

QList<PatientProfile> PatientManager::patients() const
{
    return m_patients;
}

void PatientManager::loadPatients()
{
    if (!m_apiClient || !m_apiClient->isPatientLoggedIn()) {
        emit errorOccurred("请先登录患者账号。");
        return;
    }

    common::Request request;
    request.module = "auth";
    request.action = "patientListMembers";
    m_pendingAction = request.action;
    if (!m_apiClient->send(request)) {
        m_pendingAction.clear();
    }
}

void PatientManager::addPatient(const QString& name,
                                const QString& phone,
                                const QString& idCard,
                                const QString& gender,
                                const QString& relationship)
{
    if (!m_apiClient || !m_apiClient->isPatientLoggedIn()) {
        emit errorOccurred("请先登录患者账号。");
        return;
    }

    common::Request request;
    request.module = "auth";
    request.action = "patientAddMember";
    request.payload["name"] = name.trimmed();
    request.payload["phone"] = phone.trimmed();
    request.payload["idCard"] = idCard.trimmed();
    request.payload["gender"] = gender.trimmed();
    request.payload["relationship"] = relationship.trimmed();
    m_pendingAction = request.action;
    if (!m_apiClient->send(request)) {
        m_pendingAction.clear();
    }
}

bool PatientManager::selectPatient(const QString& patientId)
{
    for (const auto& patient : m_patients) {
        if (patient.patientId == patientId) {
            m_currentPatient = patient;
            emit currentPatientChanged();
            return true;
        }
    }
    return false;
}

void PatientManager::loadMyHistory(bool onlyCurrentPatient)
{
    if (!m_apiClient || !m_apiClient->isPatientLoggedIn()) {
        emit errorOccurred("请先登录患者账号。");
        return;
    }

    common::Request request;
    request.module = "registration";
    request.action = "history";
    request.payload["patientUserId"] = m_apiClient->patientUserId();
    if (onlyCurrentPatient && hasCurrentPatient()) {
        request.payload["patientId"] = m_currentPatient.patientId;
    }
    if (!m_apiClient->send(request)) {
        emit errorOccurred("服务端未连接，请先启动服务端。");
    }
}

void PatientManager::onResponseReceived(const common::Response& response)
{
    const QString module = response.data.value("module").toString();
    const QString action = response.data.value("action").toString();
    if (module == "auth" && action == "patientListMembers") {
        if (!response.success) {
            emit errorOccurred(response.message);
            return;
        }
        setPatientsFromJson(response.data.value("rows").toArray());
        selectInitialPatient();
        emit patientsLoaded();
        return;
    }

    if (module == "auth" && action == "patientAddMember") {
        if (!response.success) {
            emit errorOccurred(response.message);
            return;
        }
        const PatientProfile patient = profileFromJson(response.data);
        if (!patient.patientId.isEmpty()) {
            m_patients.append(patient);
            m_currentPatient = patient;
            emit patientsLoaded();
            emit currentPatientChanged();
        }
        loadPatients();
        return;
    }

    if (module == "registration" && action == "history") {
        if (!response.success) {
            emit errorOccurred(response.message);
            return;
        }
        emit historyLoaded(response.data.value("rows").toArray());
    }
}

PatientProfile PatientManager::profileFromJson(const QJsonObject& object) const
{
    PatientProfile patient;
    patient.patientId = object.value("patientId").toVariant().toString();
    patient.name = object.value("name").toString(object.value("patientName").toString());
    patient.phone = object.value("phone").toString();
    patient.idCard = object.value("idCard").toString();
    patient.gender = object.value("gender").toString("未知");
    patient.relationship = object.value("relationship").toString("家属");
    return patient;
}

void PatientManager::setPatientsFromJson(const QJsonArray& rows)
{
    m_patients.clear();
    for (const auto& item : rows) {
        const auto patient = profileFromJson(item.toObject());
        if (!patient.patientId.isEmpty()) {
            m_patients.append(patient);
        }
    }
}

void PatientManager::selectInitialPatient()
{
    if (m_patients.isEmpty()) {
        m_currentPatient = {};
        emit currentPatientChanged();
        return;
    }

    const QString previousId = m_currentPatient.patientId;
    if (!previousId.isEmpty() && selectPatient(previousId)) {
        return;
    }

    const QString apiPatientId = m_apiClient ? m_apiClient->patientId() : QString();
    if (!apiPatientId.isEmpty() && selectPatient(apiPatientId)) {
        return;
    }

    m_currentPatient = m_patients.first();
    emit currentPatientChanged();
}

} // namespace hospital::client
