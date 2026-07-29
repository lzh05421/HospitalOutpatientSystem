#include "server/booking/ScheduleService.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTextStream>

namespace hospital::server::booking {
namespace {

std::string makeAppointmentNo()
{
    return ("APT" + QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmsszzz")).toStdString();
}

std::string makeEventId(const std::string& appointmentNo)
{
    return "EVT-" + appointmentNo;
}

std::string makeAppointmentCreatedPayload(const AppointmentDraft& appointment, std::int64_t appointmentId)
{
    QJsonObject payload;
    payload["appointmentId"] = QString::number(appointmentId);
    payload["appointmentNo"] = QString::fromStdString(appointment.appointmentNo);
    payload["scheduleId"] = QString::fromStdString(appointment.scheduleId);
    payload["userId"] = QString::fromStdString(appointment.userId);
    payload["requestId"] = QString::fromStdString(appointment.requestId);
    payload["status"] = QString::fromStdString(appointment.status);
    payload["occurredAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return QJsonDocument(payload).toJson(QJsonDocument::Compact).toStdString();
}

std::string makeHeaders()
{
    QJsonObject headers;
    headers["schemaVersion"] = 1;
    headers["source"] = "hospital.booking";
    return QJsonDocument(headers).toJson(QJsonDocument::Compact).toStdString();
}

void writeDemoLog(const QString& message)
{
    qInfo().noquote() << message;
    QTextStream(stdout) << message << Qt::endl;
}

} // namespace

BusinessException::BusinessException(const std::string& code)
    : std::runtime_error(code)
    , m_code(code)
{
}

const std::string& BusinessException::code() const noexcept
{
    return m_code;
}

ServiceUnavailableException::ServiceUnavailableException(const std::string& code)
    : std::runtime_error(code)
    , m_code(code)
{
}

const std::string& ServiceUnavailableException::code() const noexcept
{
    return m_code;
}

ScheduleService::ScheduleService(RedisManager& redis, IBookingRepository& repository)
    : m_redis(redis)
    , m_repository(repository)
{
}

bool ScheduleService::bookAppointment(std::string scheduleId,
                                      std::string userId,
                                      std::string requestId)
{
    if (scheduleId.empty()) {
        throw BusinessException("INVALID_SCHEDULE_ID");
    }
    if (userId.empty()) {
        throw BusinessException("INVALID_USER_ID");
    }
    if (requestId.empty()) {
        throw BusinessException("INVALID_REQUEST_ID");
    }

    bool transactionStarted = false;
    bool redisDeducted = false;
    try {
        m_repository.beginTransaction();
        transactionStarted = true;
        writeDemoLog(QStringLiteral("Redis pre-deduction start"));
        const DeductStockResult result = m_redis.deductStock(scheduleId, requestId, 1);
        switch (result) {
        case DeductStockResult::Success:
            writeDemoLog(QStringLiteral("Redis pre-deduction success"));
            redisDeducted = true;
            break;
        case DeductStockResult::DuplicateRequest:
            writeDemoLog(QStringLiteral("Redis duplicate request detected, returning idempotent success"));
            m_repository.commitTransaction();
            transactionStarted = false;
            return true;
        case DeductStockResult::InsufficientStock:
            if (transactionStarted) {
                m_repository.rollbackTransaction();
                transactionStarted = false;
            }
            throw BusinessException("OUT_OF_STOCK");
        case DeductStockResult::InvalidQuantity:
            if (transactionStarted) {
                m_repository.rollbackTransaction();
                transactionStarted = false;
            }
            throw BusinessException("INVALID_QUANTITY");
        case DeductStockResult::StockKeyMissing:
            if (transactionStarted) {
                m_repository.rollbackTransaction();
                transactionStarted = false;
            }
            throw BusinessException("STOCK_KEY_MISSING");
        }

        const AppointmentDraft appointment{
            makeAppointmentNo(),
            scheduleId,
            userId,
            requestId,
            "BOOKED"
        };
        const std::int64_t appointmentId = m_repository.insertAppointment(appointment);

        const OutboxEventDraft outboxEvent{
            makeEventId(appointment.appointmentNo),
            "AppointmentCreated",
            "Appointment",
            appointmentId,
            appointment.appointmentNo,
            "appointment.created",
            makeAppointmentCreatedPayload(appointment, appointmentId),
            makeHeaders()
        };
        m_repository.insertOutboxEvent(outboxEvent);
        m_repository.commitTransaction();
        transactionStarted = false;
        writeDemoLog(QStringLiteral("Appointment transaction committed"));
        return true;
    } catch (const RedisException& error) {
        if (transactionStarted) {
            m_repository.rollbackTransaction();
            transactionStarted = false;
        }
        qWarning().noquote()
            << "Redis appointment pre-deduction failed:"
            << QString::fromStdString(error.what())
            << "scheduleId=" << QString::fromStdString(scheduleId)
            << "requestId=" << QString::fromStdString(requestId);
        throw ServiceUnavailableException("REDIS_UNAVAILABLE");
    } catch (const BusinessException&) {
        throw;
    } catch (const std::exception& error) {
        if (transactionStarted) {
            m_repository.rollbackTransaction();
            transactionStarted = false;
        }
        if (redisDeducted) {
            try {
                const bool compensated = m_redis.compensateStockDeduct(scheduleId, requestId, 1);
                qWarning().noquote()
                    << "Redis stock compensation after DB failure"
                    << "scheduleId=" << QString::fromStdString(scheduleId)
                    << "requestId=" << QString::fromStdString(requestId)
                    << "compensated=" << compensated;
            } catch (const RedisException& compensationError) {
                qCritical().noquote()
                    << "Redis stock compensation failed:"
                    << QString::fromStdString(compensationError.what())
                    << "scheduleId=" << QString::fromStdString(scheduleId)
                    << "requestId=" << QString::fromStdString(requestId);
            }
        }
        qWarning().noquote()
            << "Appointment DB transaction failed:"
            << QString::fromStdString(error.what())
            << "scheduleId=" << QString::fromStdString(scheduleId)
            << "requestId=" << QString::fromStdString(requestId);
        throw ServiceUnavailableException("DB_TRANSACTION_FAILED");
    }
}

} // namespace hospital::server::booking
