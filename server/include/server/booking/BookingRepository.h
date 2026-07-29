#pragma once

#include <QSqlDatabase>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace hospital { namespace server { namespace outbox {
class MockOutboxStore;
}

namespace hospital { namespace server { namespace booking {

struct AppointmentDraft
{
    std::string appointmentNo;
    std::string scheduleId;
    std::string userId;
    std::string requestId;
    std::string status = "BOOKED";
};

struct OutboxEventDraft
{
    std::string eventId;
    std::string eventType;
    std::string aggregateType;
    std::int64_t aggregateId = 0;
    std::string businessKey;
    std::string routeKey;
    std::string payloadJson;
    std::string headersJson = "{}";
};

class DataAccessException : public std::runtime_error
{
public:
    explicit DataAccessException(const std::string& message);
};

class IBookingRepository
{
public:
    virtual ~IBookingRepository() = default;

    virtual void beginTransaction() = 0;
    virtual void commitTransaction() = 0;
    virtual void rollbackTransaction() noexcept = 0;
    virtual std::int64_t insertAppointment(const AppointmentDraft& appointment) = 0;
    virtual void insertOutboxEvent(const OutboxEventDraft& event) = 0;
};

class BookingRepository : public IBookingRepository
{
public:
    explicit BookingRepository(QSqlDatabase database);

    void beginTransaction() override;
    void commitTransaction() override;
    void rollbackTransaction() noexcept override;
    std::int64_t insertAppointment(const AppointmentDraft& appointment) override;
    void insertOutboxEvent(const OutboxEventDraft& event) override;

private:
    QSqlDatabase m_database;
};

class MockBookingRepository : public IBookingRepository
{
public:
    MockBookingRepository() = default;
    explicit MockBookingRepository(std::shared_ptr<outbox::MockOutboxStore> outboxStore);

    void beginTransaction() override;
    void commitTransaction() override;
    void rollbackTransaction() noexcept override;
    std::int64_t insertAppointment(const AppointmentDraft& appointment) override;
    void insertOutboxEvent(const OutboxEventDraft& event) override;

private:
    bool m_inTransaction = false;
    std::int64_t m_nextAppointmentId = 1;
    std::shared_ptr<outbox::MockOutboxStore> m_outboxStore;
};

}}} // namespace hospital::server::booking
