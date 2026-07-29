#include "server/AppConfig.h"
#include "server/DatabaseManager.h"
#include "server/HospitalServer.h"
#include "server/RequestRouter.h"
#include "server/RedisManager.h"
#include "server/SessionManager.h"
#include "server/booking/BookingRepository.h"
#include "server/booking/ScheduleService.h"
#include "server/modules/AuthService.h"
#include "server/modules/ModuleServices.h"
#include "server/outbox/OutboxConsumer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QStringList>

#include <chrono>
#include <memory>
#include <thread>

using namespace hospital::server;

namespace {

int runRedisBookingDemo(const QStringList& arguments)
{
    if (arguments.size() < 5) {
        qInfo().noquote()
            << "Usage:"
            << arguments.value(0)
            << "--redis-book-demo <scheduleId> <userId> <requestIdPrefix> [count] [scriptPath]";
        qInfo().noquote()
            << "Example:"
            << arguments.value(0)
            << "--redis-book-demo 123 user01 req-001 10 server/scripts/stock_deduct.lua";
        return 2;
    }

    const std::string scheduleId = arguments.value(2).toStdString();
    const std::string userId = arguments.value(3).toStdString();
    const std::string requestPrefix = arguments.value(4).toStdString();
    const int count = arguments.value(5, QStringLiteral("1")).toInt();
    const std::string scriptPath = arguments.value(6, QStringLiteral("server/scripts/stock_deduct.lua")).toStdString();

    const std::string redisHost = qEnvironmentVariable("HOSPITAL_REDIS_HOST", "127.0.0.1").toStdString();
    const int redisPort = qEnvironmentVariableIntValue("HOSPITAL_REDIS_PORT");
    const std::string redisPassword = qEnvironmentVariable("HOSPITAL_REDIS_PASSWORD").toStdString();
    const int redisDatabase = qEnvironmentVariableIntValue("HOSPITAL_REDIS_DB");

    RedisManager::instance().configure(redisHost,
                                       redisPort > 0 ? redisPort : 6379,
                                       redisPassword,
                                       redisDatabase,
                                       scriptPath);

    auto mockOutboxStore = std::make_shared<outbox::MockOutboxStore>();
    outbox::OutboxConsumer outboxConsumer(mockOutboxStore, 500);
    outboxConsumer.start();

    booking::MockBookingRepository bookingRepository(mockOutboxStore);
    booking::ScheduleService bookingService(RedisManager::instance(), bookingRepository);
    for (int i = 0; count <= 0 || i < count; ++i) {
        const std::string requestId = count == 1
            ? requestPrefix
            : requestPrefix + "-" + std::to_string(i + 1);

        try {
            const bool booked = bookingService.bookAppointment(scheduleId, userId, requestId);
            qInfo().noquote()
                << "bookAppointment"
                << "scheduleId=" << QString::fromStdString(scheduleId)
                << "requestId=" << QString::fromStdString(requestId)
                << "result=" << booked;
        } catch (const booking::BusinessException& error) {
            qWarning().noquote()
                << "bookAppointment business rejected:"
                << QString::fromStdString(error.code())
                << "requestId=" << QString::fromStdString(requestId);
        } catch (const booking::ServiceUnavailableException& error) {
            qCritical().noquote()
                << "bookAppointment service unavailable:"
                << QString::fromStdString(error.code());
            return 3;
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    outboxConsumer.stop();
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();

    if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--redis-book-demo")) {
        return runRedisBookingDemo(arguments);
    }

    const bool realDbMode = arguments.contains(QStringLiteral("--real-db-mode"));
    QString configPath = QStringLiteral("server.ini");
    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (!argument.startsWith(QStringLiteral("--"))) {
            configPath = argument;
            break;
        }
    }

    AppConfig config = AppConfig::load(configPath);
    if (realDbMode) {
        config.databaseEnabled = true;
        qInfo() << "Real DB mode enabled; OutboxConsumer will use configured MySQL connection.";
    }

    DatabaseManager database;
    if (!database.open(config)) {
        qWarning() << "Failed to connect MySQL:" << database.lastError();
        if (realDbMode) {
            qCritical() << "Real DB mode requires a working MySQL connection.";
            return 1;
        }
        qWarning() << "Server will keep listening, and the client will show the database error.";
    }
    if (!database.isEnabled()) {
        qInfo() << "Database disabled, running with built-in demo data.";
    }

    RequestRouter router;
    router.setDatabase(&database);
    SessionManager sessions;
    router.setSessionManager(&sessions);

    AuthService authService(&database, &sessions);
    PatientService patientService(&database);
    RegistrationService registrationService(&database);
    DepartmentService departmentService(&database);
    ScheduleService scheduleService(&database);
    DoctorService doctorService(&database);
    ConsultationService consultationService(&database);
    ExaminationService examinationService(&database);
    PrescriptionService prescriptionService(&database);
    InventoryService inventoryService(&database);
    BillingService billingService(&database);
    StatisticsService statisticsService(&database);
    DashboardService dashboardService(&database);
    PatientRecordService patientRecordService(&database);
    OperationLogService operationLogService(&database);
    PermissionAdminService permissionAdminService(&database);

    router.registerService("auth", &authService);
    router.registerService("patient", &patientService);
    router.registerService("registration", &registrationService);
    router.registerService("department", &departmentService);
    router.registerService("schedule", &scheduleService);
    router.registerService("doctor", &doctorService);
    router.registerService("consultation", &consultationService);
    router.registerService("examination", &examinationService);
    router.registerService("prescription", &prescriptionService);
    router.registerService("inventory", &inventoryService);
    router.registerService("billing", &billingService);
    router.registerService("statistics", &statisticsService);
    router.registerService("dashboard", &dashboardService);
    router.registerService("patientRecord", &patientRecordService);
    router.registerService("operationLog", &operationLogService);
    router.registerService("permissionAdmin", &permissionAdminService);

    HospitalServer server(&router);
    if (!server.listen(config)) {
        qCritical() << "Failed to listen on" << config.serverHost << config.serverPort;
        return 1;
    }

    std::unique_ptr<outbox::OutboxConsumer> outboxConsumer;
    if (database.isEnabled() && database.isOpen()) {
        outboxConsumer = std::make_unique<outbox::OutboxConsumer>(database.database(), 1000);
        outboxConsumer->start();
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [&outboxConsumer]() {
            if (outboxConsumer) {
                outboxConsumer->stop();
            }
        });
    }

    qInfo() << "Hospital server listening on" << config.serverHost << config.serverPort;
    const int exitCode = app.exec();
    if (outboxConsumer) {
        outboxConsumer->stop();
    }
    return exitCode;
}
