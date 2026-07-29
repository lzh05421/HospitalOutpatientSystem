#include "server/booking/BookingRepository.h"

#include "server/outbox/OutboxConsumer.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

#include <utility>

namespace hospital::server::booking {
namespace {

QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}

std::string sqlError(const QString& action, const QSqlError& error)
{
    return (action + QStringLiteral(": ") + error.text()).toStdString();
}

} // namespace

DataAccessException::DataAccessException(const std::string& message)
    : std::runtime_error(message)
{
}

MockBookingRepository::MockBookingRepository(std::shared_ptr<outbox::MockOutboxStore> outboxStore)
    : m_outboxStore(std::move(outboxStore))
{
}

BookingRepository::BookingRepository(QSqlDatabase database)
    : m_database(std::move(database))
{
}

void BookingRepository::beginTransaction()
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        throw DataAccessException("database connection is not open");
    }
    if (!m_database.transaction()) {
        throw DataAccessException(sqlError(QStringLiteral("BEGIN transaction failed"), m_database.lastError()));
    }
}

void BookingRepository::commitTransaction()
{
    if (!m_database.commit()) {
        throw DataAccessException(sqlError(QStringLiteral("COMMIT transaction failed"), m_database.lastError()));
    }
}

void BookingRepository::rollbackTransaction() noexcept
{
    if (m_database.isValid() && m_database.isOpen()) {
        m_database.rollback();
    }
}

std::int64_t BookingRepository::insertAppointment(const AppointmentDraft& appointment)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO appointments "
                  "(appointment_no, schedule_id, user_id, request_id, status, created_at) "
                  "VALUES (?, ?, ?, ?, ?, NOW())");
    query.addBindValue(toQString(appointment.appointmentNo));
    query.addBindValue(toQString(appointment.scheduleId));
    query.addBindValue(toQString(appointment.userId));
    query.addBindValue(toQString(appointment.requestId));
    query.addBindValue(toQString(appointment.status));

    if (!query.exec()) {
        throw DataAccessException(sqlError(QStringLiteral("insert appointment failed"), query.lastError()));
    }

    const QVariant id = query.lastInsertId();
    if (!id.isValid()) {
        throw DataAccessException("insert appointment did not return an id");
    }
    return id.toLongLong();
}

void BookingRepository::insertOutboxEvent(const OutboxEventDraft& event)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO outbox_events "
                  "(event_id, event_type, aggregate_type, aggregate_id, business_key, route_key, payload, headers, status) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)");
    query.addBindValue(toQString(event.eventId));
    query.addBindValue(toQString(event.eventType));
    query.addBindValue(toQString(event.aggregateType));
    query.addBindValue(QVariant::fromValue<qlonglong>(event.aggregateId));
    query.addBindValue(toQString(event.businessKey));
    query.addBindValue(toQString(event.routeKey));
    query.addBindValue(toQString(event.payloadJson));
    query.addBindValue(toQString(event.headersJson));

    if (!query.exec()) {
        throw DataAccessException(sqlError(QStringLiteral("insert outbox event failed"), query.lastError()));
    }
}

void MockBookingRepository::beginTransaction()
{
    qInfo() << "DB transaction begin";
    QTextStream(stdout) << "DB transaction begin" << Qt::endl;
    m_inTransaction = true;
}

void MockBookingRepository::commitTransaction()
{
    if (!m_inTransaction) {
        throw DataAccessException("mock commit without active transaction");
    }
    qInfo() << "DB transaction commit";
    QTextStream(stdout) << "DB transaction commit" << Qt::endl;
    m_inTransaction = false;
}

void MockBookingRepository::rollbackTransaction() noexcept
{
    if (m_inTransaction) {
        qInfo() << "DB transaction rollback";
        QTextStream(stdout) << "DB transaction rollback" << Qt::endl;
    }
    m_inTransaction = false;
}

std::int64_t MockBookingRepository::insertAppointment(const AppointmentDraft& appointment)
{
    if (!m_inTransaction) {
        throw DataAccessException("mock insert appointment without active transaction");
    }

    const std::int64_t appointmentId = m_nextAppointmentId++;
    qInfo().noquote()
        << "Insert appointment"
        << "id=" << appointmentId
        << "appointmentNo=" << toQString(appointment.appointmentNo)
        << "scheduleId=" << toQString(appointment.scheduleId)
        << "userId=" << toQString(appointment.userId)
        << "requestId=" << toQString(appointment.requestId);
    QTextStream(stdout)
        << "Insert appointment"
        << " id=" << appointmentId
        << " appointmentNo=" << toQString(appointment.appointmentNo)
        << " scheduleId=" << toQString(appointment.scheduleId)
        << " userId=" << toQString(appointment.userId)
        << " requestId=" << toQString(appointment.requestId)
        << Qt::endl;
    return appointmentId;
}

void MockBookingRepository::insertOutboxEvent(const OutboxEventDraft& event)
{
    if (!m_inTransaction) {
        throw DataAccessException("mock insert outbox event without active transaction");
    }

    qInfo().noquote()
        << "Insert outbox event"
        << "eventId=" << toQString(event.eventId)
        << "eventType=" << toQString(event.eventType)
        << "businessKey=" << toQString(event.businessKey)
        << "payload=" << toQString(event.payloadJson);
    QTextStream(stdout)
        << "Insert outbox event"
        << " eventId=" << toQString(event.eventId)
        << " eventType=" << toQString(event.eventType)
        << " businessKey=" << toQString(event.businessKey)
        << " payload=" << toQString(event.payloadJson)
        << Qt::endl;

    if (m_outboxStore) {
        m_outboxStore->append(event.payloadJson);
    }
}

} // namespace hospital::server::booking
