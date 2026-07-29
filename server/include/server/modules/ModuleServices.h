#pragma once

#include "server/RequestRouter.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace hospital { namespace server {

class DatabaseManager;

class PatientService : public ModuleService
{
public:
    explicit PatientService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class RegistrationService : public ModuleService
{
public:
    explicit RegistrationService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class ScheduleService : public ModuleService
{
public:
    explicit ScheduleService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class DepartmentService : public ModuleService
{
public:
    explicit DepartmentService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class DoctorService : public ModuleService
{
public:
    explicit DoctorService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class ConsultationService : public ModuleService
{
public:
    explicit ConsultationService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class PrescriptionService : public ModuleService
{
public:
    explicit PrescriptionService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class ExaminationService : public ModuleService
{
public:
    explicit ExaminationService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class InventoryService : public ModuleService
{
public:
    explicit InventoryService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class BillingService : public ModuleService
{
public:
    explicit BillingService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;

private:
    struct InsuranceApiResult
    {
        bool accepted = false;
        QString transactionId;
        QString message;
    };

    common::Response updateBill(const QJsonObject& payload);
    common::Response cancelBill(const QJsonObject& payload);
    common::Response processSelfPay(const QJsonObject& payload);
    common::Response processMedicalInsurancePay(const QJsonObject& payload);
    common::Response createPaymentQr(const QJsonObject& payload);
    common::Response checkPayStatus(const QJsonObject& payload);
    common::Response handleInsuranceCallback(const QJsonObject& payload);
    common::Response requestRefundBill(const QJsonObject& payload);
    common::Response reviewRefundBill(const QJsonObject& payload);
    InsuranceApiResult callMedicalInsuranceAPI(const QString& transactionId,
                                               qint64 billId,
                                               qint64 patientId,
                                               double totalAmount,
                                               const QJsonArray& items);
    void logPaymentError(const QString& billNo,
                         const QString& action,
                         const QString& message,
                         const QString& operatorId = QString()) const;

private:
    DatabaseManager* m_database = nullptr;
};

class StatisticsService : public ModuleService
{
public:
    explicit StatisticsService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class DashboardService : public ModuleService
{
public:
    explicit DashboardService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class PatientRecordService : public ModuleService
{
public:
    explicit PatientRecordService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class OperationLogService : public ModuleService
{
public:
    explicit OperationLogService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

class PermissionAdminService : public ModuleService
{
public:
    explicit PermissionAdminService(DatabaseManager* database);
    common::Response handle(const common::Request& request) override;
private:
    DatabaseManager* m_database = nullptr;
};

}} // namespace hospital::server
