#pragma once

#include "server/booking/BookingRepository.h"
#include "server/RedisManager.h"

#include <stdexcept>
#include <string>

namespace hospital { namespace server { namespace booking {

class BusinessException : public std::runtime_error
{
public:
    explicit BusinessException(const std::string& code);

    const std::string& code() const noexcept;

private:
    std::string m_code;
};

class ServiceUnavailableException : public std::runtime_error
{
public:
    explicit ServiceUnavailableException(const std::string& code);

    const std::string& code() const noexcept;

private:
    std::string m_code;
};

class ScheduleService
{
public:
    ScheduleService(RedisManager& redis, IBookingRepository& repository);

    bool bookAppointment(std::string scheduleId,
                         std::string userId,
                         std::string requestId);

private:
    RedisManager& m_redis;
    IBookingRepository& m_repository;
};

}}} // namespace hospital::server::booking
