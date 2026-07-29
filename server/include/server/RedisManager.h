#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

struct redisContext;

namespace hospital { namespace server {

class RedisException : public std::runtime_error
{
public:
    explicit RedisException(const std::string& message);
};

enum class DeductStockResult
{
    Success = 1,
    DuplicateRequest = 0,
    InsufficientStock = -1,
    InvalidQuantity = -2,
    StockKeyMissing = -3
};

class RedisManager
{
public:
    static RedisManager& instance();

    RedisManager(const RedisManager&) = delete;
    RedisManager& operator=(const RedisManager&) = delete;

    void configure(std::string host,
                   int port,
                   std::string password,
                   int database,
                   std::string stockDeductScriptPath);

    void disconnect();

    DeductStockResult deductStock(const std::string& scheduleId,
                                  const std::string& requestId,
                                  int amount);

    bool compensateStockDeduct(const std::string& scheduleId,
                               const std::string& requestId,
                               int amount);

private:
    RedisManager() = default;
    ~RedisManager();

    void ensureConnectedLocked();
    void disconnectLocked();
    void loadStockDeductScriptLocked();
    DeductStockResult runStockDeductScriptLocked(const std::string& stockKey,
                                                 const std::string& requestId,
                                                 int amount,
                                                 bool preferEvalSha);
    bool runStockCompensationScriptLocked(const std::string& stockKey,
                                          const std::string& requestId,
                                          int amount);
    DeductStockResult parseDeductResult(long long code) const;

    static std::string readTextFile(const std::string& path);
    static std::string buildScheduleStockKey(const std::string& scheduleId);

    std::mutex m_mutex;
    redisContext* m_context = nullptr;
    std::string m_host = "127.0.0.1";
    int m_port = 6379;
    std::string m_password;
    int m_database = 0;
    std::string m_stockDeductScriptPath = "server/scripts/stock_deduct.lua";
    std::string m_stockDeductScriptBody;
    std::string m_stockDeductScriptSha;
};

}} // namespace hospital::server
