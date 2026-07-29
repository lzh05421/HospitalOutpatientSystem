#pragma once

#include <QSqlDatabase>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace hospital { namespace server { namespace outbox {

enum class OutboxStatus
{
    Pending = 0,
    Publishing = 1,
    Processed = 2,
    Failed = 3
};

struct MockOutboxRecord
{
    std::int64_t id = 0;
    int retryCount = 0;
    OutboxStatus status = OutboxStatus::Pending;
    std::string payloadJson;
    std::string lastError;
};

class MockOutboxStore
{
public:
    std::int64_t append(std::string payloadJson);
    std::vector<MockOutboxRecord> pendingBatch(std::size_t limit);
    void markProcessed(std::int64_t id);
    void markRetryOrFailed(std::int64_t id, std::string error, int maxRetry);
    OutboxStatus statusOf(std::int64_t id) const;

private:
    mutable std::mutex m_mutex;
    std::int64_t m_nextId = 1;
    std::vector<MockOutboxRecord> m_records;
};

class OutboxConsumer
{
public:
    explicit OutboxConsumer(QSqlDatabase database, int pollIntervalMs = 1000);
    explicit OutboxConsumer(std::shared_ptr<MockOutboxStore> mockStore, int pollIntervalMs = 1000);
    ~OutboxConsumer();

    OutboxConsumer(const OutboxConsumer&) = delete;
    OutboxConsumer& operator=(const OutboxConsumer&) = delete;

    void start();
    void stop();
    void pollAndProcess();

private:
    struct EventRecord
    {
        std::int64_t id = 0;
        int retryCount = 0;
        std::string payloadJson;
    };

    void runLoop();
    void pollAndProcessSql();
    void pollAndProcessMock();
    std::vector<EventRecord> fetchPendingSql(QSqlDatabase& database);
    void markSqlProcessed(QSqlDatabase& database, std::int64_t id);
    void markSqlRetryOrFailed(QSqlDatabase& database, std::int64_t id, int retryCount, const std::string& error);
    void processEvent(const EventRecord& record);
    QSqlDatabase databaseForCurrentThread();
    void closeThreadDatabase();

    std::atomic<bool> m_running{false};
    std::thread m_thread;
    int m_pollIntervalMs = 1000;
    int m_maxRetry = 3;
    std::string m_sourceConnectionName;
    std::string m_threadConnectionName;
    std::shared_ptr<MockOutboxStore> m_mockStore;
};

}}} // namespace hospital::server::outbox
