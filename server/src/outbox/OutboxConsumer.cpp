#include "server/outbox/OutboxConsumer.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QThread>
#include <QVariant>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace hospital::server::outbox {
namespace {

constexpr int kPendingStatus = static_cast<int>(OutboxStatus::Pending);
constexpr int kProcessedStatus = static_cast<int>(OutboxStatus::Processed);
constexpr int kFailedStatus = static_cast<int>(OutboxStatus::Failed);

QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}

std::string userIdFromPayload(const std::string& payloadJson)
{
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(payloadJson));
    if (!document.isObject()) {
        throw std::runtime_error("outbox payload is not a JSON object");
    }

    const QJsonObject payload = document.object();
    const QString userId = payload.value("userId").toString();
    if (userId.isEmpty()) {
        throw std::runtime_error("outbox payload missing userId");
    }
    return userId.toStdString();
}

void writeProcessingLog(const std::string& userId)
{
    const QString message = QStringLiteral("Processing appointment for user: %1").arg(toQString(userId));
    qInfo().noquote() << message;
    QTextStream(stdout) << message << Qt::endl;
}

void throwSqlError(const QString& action, const QSqlError& error)
{
    throw std::runtime_error((action + QStringLiteral(": ") + error.text()).toStdString());
}

bool isConnectionError(const QSqlError& error)
{
    return error.type() == QSqlError::ConnectionError;
}

void reopenDatabase(QSqlDatabase& database)
{
    database.close();
    if (!database.open()) {
        throwSqlError(QStringLiteral("reopen outbox consumer database failed"), database.lastError());
    }
}

} // namespace

std::int64_t MockOutboxStore::append(std::string payloadJson)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::int64_t id = m_nextId++;
    m_records.push_back(MockOutboxRecord{id, 0, OutboxStatus::Pending, std::move(payloadJson), {}});
    return id;
}

std::vector<MockOutboxRecord> MockOutboxStore::pendingBatch(std::size_t limit)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MockOutboxRecord> batch;
    for (const auto& record : m_records) {
        if (record.status == OutboxStatus::Pending) {
            batch.push_back(record);
            if (batch.size() >= limit) {
                break;
            }
        }
    }
    return batch;
}

void MockOutboxStore::markProcessed(std::int64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& record : m_records) {
        if (record.id == id) {
            record.status = OutboxStatus::Processed;
            record.lastError.clear();
            return;
        }
    }
}

void MockOutboxStore::markRetryOrFailed(std::int64_t id, std::string error, int maxRetry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& record : m_records) {
        if (record.id == id) {
            record.retryCount += 1;
            record.lastError = std::move(error);
            if (record.retryCount >= maxRetry) {
                record.status = OutboxStatus::Failed;
            }
            return;
        }
    }
}

OutboxStatus MockOutboxStore::statusOf(std::int64_t id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& record : m_records) {
        if (record.id == id) {
            return record.status;
        }
    }
    throw std::runtime_error("mock outbox record not found");
}

OutboxConsumer::OutboxConsumer(QSqlDatabase database, int pollIntervalMs)
    : m_pollIntervalMs(pollIntervalMs)
    , m_sourceConnectionName(database.connectionName().toStdString())
{
}

OutboxConsumer::OutboxConsumer(std::shared_ptr<MockOutboxStore> mockStore, int pollIntervalMs)
    : m_pollIntervalMs(pollIntervalMs)
    , m_mockStore(std::move(mockStore))
{
}

OutboxConsumer::~OutboxConsumer()
{
    stop();
}

void OutboxConsumer::start()
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;
    }
    m_thread = std::thread(&OutboxConsumer::runLoop, this);
}

void OutboxConsumer::stop()
{
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void OutboxConsumer::pollAndProcess()
{
    if (m_mockStore) {
        pollAndProcessMock();
        return;
    }
    pollAndProcessSql();
}

void OutboxConsumer::runLoop()
{
    while (m_running.load()) {
        try {
            pollAndProcess();
        } catch (const std::exception& error) {
            qWarning().noquote() << "Outbox consumer polling failed:" << error.what();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(m_pollIntervalMs));
    }
    closeThreadDatabase();
}

void OutboxConsumer::pollAndProcessSql()
{
    QSqlDatabase database = databaseForCurrentThread();
    const auto events = fetchPendingSql(database);
    for (const auto& event : events) {
        try {
            processEvent(event);
            markSqlProcessed(database, event.id);
        } catch (const std::exception& error) {
            markSqlRetryOrFailed(database, event.id, event.retryCount, error.what());
        }
    }
}

void OutboxConsumer::pollAndProcessMock()
{
    const auto events = m_mockStore->pendingBatch(10);
    for (const auto& event : events) {
        try {
            processEvent(EventRecord{event.id, event.retryCount, event.payloadJson});
            m_mockStore->markProcessed(event.id);
        } catch (const std::exception& error) {
            m_mockStore->markRetryOrFailed(event.id, error.what(), m_maxRetry);
        }
    }
}

std::vector<OutboxConsumer::EventRecord> OutboxConsumer::fetchPendingSql(QSqlDatabase& database)
{
    auto prepareQuery = [&database]() {
        QSqlQuery query(database);
        query.prepare("SELECT id, retry_count, payload "
                      "FROM outbox_events "
                      "WHERE status = ? "
                      "ORDER BY created_at ASC "
                      "LIMIT 10");
        query.addBindValue(kPendingStatus);
        return query;
    };

    QSqlQuery query = prepareQuery();
    if (!query.exec()) {
        const QSqlError error = query.lastError();
        if (isConnectionError(error)) {
            reopenDatabase(database);
            query = prepareQuery();
            if (!query.exec()) {
                throwSqlError(QStringLiteral("fetch pending outbox events failed after reconnect"), query.lastError());
            }
        } else {
            throwSqlError(QStringLiteral("fetch pending outbox events failed"), error);
        }
    }

    std::vector<EventRecord> events;
    while (query.next()) {
        EventRecord event;
        event.id = query.value(0).toLongLong();
        event.retryCount = query.value(1).toInt();
        event.payloadJson = query.value(2).toString().toStdString();
        events.push_back(std::move(event));
    }
    return events;
}

void OutboxConsumer::markSqlProcessed(QSqlDatabase& database, std::int64_t id)
{
    auto prepareQuery = [&database, id]() {
        QSqlQuery query(database);
        query.prepare("UPDATE outbox_events "
                      "SET status = ?, published_at = NOW(3), updated_at = NOW(3), last_error = NULL "
                      "WHERE id = ?");
        query.addBindValue(kProcessedStatus);
        query.addBindValue(QVariant::fromValue<qlonglong>(id));
        return query;
    };

    QSqlQuery query = prepareQuery();
    if (!query.exec()) {
        const QSqlError error = query.lastError();
        if (isConnectionError(error)) {
            reopenDatabase(database);
            query = prepareQuery();
            if (!query.exec()) {
                throwSqlError(QStringLiteral("mark outbox event processed failed after reconnect"), query.lastError());
            }
        } else {
            throwSqlError(QStringLiteral("mark outbox event processed failed"), error);
        }
    }
}

void OutboxConsumer::markSqlRetryOrFailed(QSqlDatabase& database,
                                          std::int64_t id,
                                          int retryCount,
                                          const std::string& error)
{
    const int nextRetryCount = retryCount + 1;
    const bool failed = nextRetryCount >= m_maxRetry;

    auto prepareQuery = [&database, id, nextRetryCount, failed, &error]() {
        QSqlQuery query(database);
        query.prepare("UPDATE outbox_events "
                      "SET status = ?, retry_count = ?, updated_at = NOW(3), last_error = ? "
                      "WHERE id = ?");
        query.addBindValue(failed ? kFailedStatus : kPendingStatus);
        query.addBindValue(nextRetryCount);
        query.addBindValue(toQString(error).left(1000));
        query.addBindValue(QVariant::fromValue<qlonglong>(id));
        return query;
    };

    QSqlQuery query = prepareQuery();
    if (!query.exec()) {
        const QSqlError sqlError = query.lastError();
        if (isConnectionError(sqlError)) {
            reopenDatabase(database);
            query = prepareQuery();
            if (!query.exec()) {
                throwSqlError(QStringLiteral("mark outbox event retry/failed failed after reconnect"), query.lastError());
            }
        } else {
            throwSqlError(QStringLiteral("mark outbox event retry/failed failed"), sqlError);
        }
    }
}

void OutboxConsumer::processEvent(const EventRecord& record)
{
    const std::string userId = userIdFromPayload(record.payloadJson);
    writeProcessingLog(userId);
}

QSqlDatabase OutboxConsumer::databaseForCurrentThread()
{
    if (m_sourceConnectionName.empty()) {
        throw std::runtime_error("outbox consumer has no database connection name");
    }

    if (m_threadConnectionName.empty()) {
        m_threadConnectionName = "outbox_consumer_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this))
            + "_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(QThread::currentThreadId()));
        QSqlDatabase::cloneDatabase(QString::fromStdString(m_sourceConnectionName),
                                    QString::fromStdString(m_threadConnectionName));
    }

    QSqlDatabase database = QSqlDatabase::database(QString::fromStdString(m_threadConnectionName));
    if (!database.isOpen() && !database.open()) {
        throwSqlError(QStringLiteral("open outbox consumer database failed"), database.lastError());
    }
    return database;
}

void OutboxConsumer::closeThreadDatabase()
{
    if (m_threadConnectionName.empty()) {
        return;
    }

    const QString connectionName = QString::fromStdString(m_threadConnectionName);
    {
        QSqlDatabase database = QSqlDatabase::database(connectionName, false);
        if (database.isValid()) {
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    m_threadConnectionName.clear();
}

} // namespace hospital::server::outbox
