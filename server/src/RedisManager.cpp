#include "server/RedisManager.h"

#ifdef HOSPITAL_ENABLE_REDIS

#include <hiredis.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

namespace hospital::server {
namespace {

struct RedisReplyDeleter
{
    void operator()(redisReply* reply) const
    {
        if (reply) {
            freeReplyObject(reply);
        }
    }
};

using RedisReplyPtr = std::unique_ptr<redisReply, RedisReplyDeleter>;

bool isNoScriptError(const redisReply* reply)
{
    return reply
        && reply->type == REDIS_REPLY_ERROR
        && reply->str
        && std::strncmp(reply->str, "NOSCRIPT", 8) == 0;
}

std::string replyErrorText(const redisReply* reply)
{
    if (!reply) {
        return "Redis command returned null reply";
    }
    if (reply->type == REDIS_REPLY_ERROR && reply->str) {
        return reply->str;
    }
    std::ostringstream stream;
    stream << "Unexpected Redis reply type: " << reply->type;
    return stream.str();
}

} // namespace

RedisException::RedisException(const std::string& message)
    : std::runtime_error(message)
{
}

RedisManager::~RedisManager()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnectLocked();
}

RedisManager& RedisManager::instance()
{
    static RedisManager manager;
    return manager;
}

void RedisManager::configure(std::string host,
                             int port,
                             std::string password,
                             int database,
                             std::string stockDeductScriptPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = std::move(host);
    m_port = port;
    m_password = std::move(password);
    m_database = database;
    m_stockDeductScriptPath = std::move(stockDeductScriptPath);
    m_stockDeductScriptBody.clear();
    m_stockDeductScriptSha.clear();
    disconnectLocked();
}

void RedisManager::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnectLocked();
}

void RedisManager::disconnectLocked()
{
    if (m_context) {
        redisFree(m_context);
        m_context = nullptr;
    }
}

DeductStockResult RedisManager::deductStock(const std::string& scheduleId,
                                            const std::string& requestId,
                                            int amount)
{
    if (scheduleId.empty()) {
        throw RedisException("scheduleId is empty");
    }
    if (requestId.empty()) {
        throw RedisException("requestId is empty");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    ensureConnectedLocked();
    loadStockDeductScriptLocked();

    const std::string stockKey = buildScheduleStockKey(scheduleId);
    try {
        return runStockDeductScriptLocked(stockKey, requestId, amount, true);
    } catch (const RedisException&) {
        throw;
    }
}

bool RedisManager::compensateStockDeduct(const std::string& scheduleId,
                                         const std::string& requestId,
                                         int amount)
{
    if (scheduleId.empty()) {
        throw RedisException("scheduleId is empty");
    }
    if (requestId.empty()) {
        throw RedisException("requestId is empty");
    }
    if (amount <= 0) {
        throw RedisException("compensation amount must be positive");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    ensureConnectedLocked();

    const std::string stockKey = buildScheduleStockKey(scheduleId);
    return runStockCompensationScriptLocked(stockKey, requestId, amount);
}

void RedisManager::ensureConnectedLocked()
{
    if (m_context && m_context->err == 0) {
        return;
    }

    disconnectLocked();

    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    m_context = redisConnectWithTimeout(m_host.c_str(), m_port, timeout);
    if (!m_context) {
        throw RedisException("Failed to allocate Redis context");
    }
    if (m_context->err) {
        const std::string error = m_context->errstr;
        disconnectLocked();
        throw RedisException("Failed to connect Redis: " + error);
    }

    if (!m_password.empty()) {
        RedisReplyPtr authReply(static_cast<redisReply*>(
            redisCommand(m_context, "AUTH %s", m_password.c_str())));
        if (!authReply || authReply->type == REDIS_REPLY_ERROR) {
            throw RedisException("Redis AUTH failed: " + replyErrorText(authReply.get()));
        }
    }

    RedisReplyPtr nameReply(static_cast<redisReply*>(
        redisCommand(m_context, "CLIENT SETNAME hospital-outpatient-server")));
    if (!nameReply || nameReply->type == REDIS_REPLY_ERROR) {
        throw RedisException("Failed to set Redis client name: " + replyErrorText(nameReply.get()));
    }

    if (m_database > 0) {
        RedisReplyPtr selectReply(static_cast<redisReply*>(
            redisCommand(m_context, "SELECT %d", m_database)));
        if (!selectReply || selectReply->type == REDIS_REPLY_ERROR) {
            throw RedisException("Redis SELECT failed: " + replyErrorText(selectReply.get()));
        }
    }
}

void RedisManager::loadStockDeductScriptLocked()
{
    if (!m_stockDeductScriptSha.empty()) {
        return;
    }

    if (m_stockDeductScriptBody.empty()) {
        m_stockDeductScriptBody = readTextFile(m_stockDeductScriptPath);
    }

    RedisReplyPtr reply(static_cast<redisReply*>(
        redisCommand(m_context,
                     "SCRIPT LOAD %b",
                     m_stockDeductScriptBody.data(),
                     m_stockDeductScriptBody.size())));
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        throw RedisException("SCRIPT LOAD failed: " + replyErrorText(reply.get()));
    }
    if (reply->type != REDIS_REPLY_STRING || !reply->str) {
        throw RedisException("SCRIPT LOAD returned an invalid SHA reply");
    }

    m_stockDeductScriptSha.assign(reply->str, static_cast<std::size_t>(reply->len));
}

DeductStockResult RedisManager::runStockDeductScriptLocked(const std::string& stockKey,
                                                           const std::string& requestId,
                                                           int amount,
                                                           bool preferEvalSha)
{
    RedisReplyPtr reply;
    if (preferEvalSha && !m_stockDeductScriptSha.empty()) {
        reply.reset(static_cast<redisReply*>(
            redisCommand(m_context,
                         "EVALSHA %s 1 %s %s %d",
                         m_stockDeductScriptSha.c_str(),
                         stockKey.c_str(),
                         requestId.c_str(),
                         amount)));

        if (isNoScriptError(reply.get())) {
            m_stockDeductScriptSha.clear();
            loadStockDeductScriptLocked();
            return runStockDeductScriptLocked(stockKey, requestId, amount, false);
        }
    } else {
        reply.reset(static_cast<redisReply*>(
            redisCommand(m_context,
                         "EVAL %b 1 %s %s %d",
                         m_stockDeductScriptBody.data(),
                         m_stockDeductScriptBody.size(),
                         stockKey.c_str(),
                         requestId.c_str(),
                         amount)));
    }

    if (!reply) {
        const std::string error = m_context ? m_context->errstr : "Redis context is null";
        disconnectLocked();
        throw RedisException("Redis script command failed: " + error);
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        throw RedisException("Redis script returned error: " + replyErrorText(reply.get()));
    }
    if (reply->type != REDIS_REPLY_INTEGER) {
        throw RedisException("Redis script returned non-integer reply");
    }

    return parseDeductResult(reply->integer);
}

bool RedisManager::runStockCompensationScriptLocked(const std::string& stockKey,
                                                    const std::string& requestId,
                                                    int amount)
{
    static const char* compensationScript =
        "local stockKey = KEYS[1]\n"
        "local requestId = ARGV[1]\n"
        "local amount = tonumber(ARGV[2])\n"
        "if amount == nil or amount <= 0 then return -1 end\n"
        "local idempotencyHashKey = stockKey .. ':deduct:requests'\n"
        "local previousResult = redis.call('HGET', idempotencyHashKey, requestId)\n"
        "if previousResult == false then return 0 end\n"
        "redis.call('INCRBY', stockKey, amount)\n"
        "redis.call('HDEL', idempotencyHashKey, requestId)\n"
        "return 1\n";

    RedisReplyPtr reply(static_cast<redisReply*>(
        redisCommand(m_context,
                     "EVAL %b 1 %s %s %d",
                     compensationScript,
                     std::strlen(compensationScript),
                     stockKey.c_str(),
                     requestId.c_str(),
                     amount)));

    if (!reply) {
        const std::string error = m_context ? m_context->errstr : "Redis context is null";
        disconnectLocked();
        throw RedisException("Redis stock compensation failed: " + error);
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        throw RedisException("Redis stock compensation returned error: " + replyErrorText(reply.get()));
    }
    if (reply->type != REDIS_REPLY_INTEGER) {
        throw RedisException("Redis stock compensation returned non-integer reply");
    }
    if (reply->integer < 0) {
        throw RedisException("Redis stock compensation rejected invalid amount");
    }

    return reply->integer == 1;
}

DeductStockResult RedisManager::parseDeductResult(long long code) const
{
    switch (code) {
    case 1:
        return DeductStockResult::Success;
    case 0:
        return DeductStockResult::DuplicateRequest;
    case -1:
        return DeductStockResult::InsufficientStock;
    case -2:
        return DeductStockResult::InvalidQuantity;
    case -3:
        return DeductStockResult::StockKeyMissing;
    default:
        throw RedisException("Unknown stock deduction result code: " + std::to_string(code));
    }
}

std::string RedisManager::readTextFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        throw RedisException("Failed to open Lua script file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();
    if (content.empty()) {
        throw RedisException("Lua script file is empty: " + path);
    }
    return content;
}

std::string RedisManager::buildScheduleStockKey(const std::string& scheduleId)
{
    return "schedule:" + scheduleId + ":remain";
}

} // namespace hospital::server

#else

namespace hospital::server {

RedisException::RedisException(const std::string& message)
    : std::runtime_error(message)
{
}

RedisManager::~RedisManager() = default;

RedisManager& RedisManager::instance()
{
    static RedisManager manager;
    return manager;
}

void RedisManager::configure(std::string,
                             int,
                             std::string,
                             int,
                             std::string)
{
}

void RedisManager::disconnect()
{
}

DeductStockResult RedisManager::deductStock(const std::string&,
                                            const std::string&,
                                            int)
{
    throw RedisException("Redis support is disabled in this build");
}

bool RedisManager::compensateStockDeduct(const std::string&,
                                         const std::string&,
                                         int)
{
    throw RedisException("Redis support is disabled in this build");
}

} // namespace hospital::server

#endif
