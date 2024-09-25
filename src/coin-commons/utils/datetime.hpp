/**
 * @file datetime.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once
#include <string>
#include <time.h>
#include <stdint.h>

namespace coin
{
class DateTime
{
public:
    DateTime() noexcept;
    DateTime(const uint64_t &t) noexcept;
    ~DateTime() noexcept;
public:
    static DateTime current_date_time();
    uint64_t to_secs_since_epoch() const noexcept;
    uint64_t to_msecs_since_epoch() const noexcept;
    uint64_t to_usecs_since_epoch() const noexcept;
    uint64_t to_nsecs_since_epoch() const noexcept;
    struct tm to_tm() const noexcept;
    uint64_t sec() const noexcept;
    uint64_t msec() const noexcept;
    uint64_t usec() const noexcept;
    uint64_t nsec() const noexcept;
    uint64_t value() const noexcept;
    int symbol() const noexcept;
    void set_date_time(uint64_t timestamp) noexcept; //ns
    void set_date_time_sec(uint64_t timestamp) noexcept; //s
    void set_date_time_msec(uint64_t timestamp) noexcept; //ms
    void set_date_time_usec(uint64_t timestamp) noexcept; //us
    void set_date_time_nsec(uint64_t timestamp) noexcept; //ns
    DateTime operator - (const DateTime &rhs) const noexcept;
    DateTime operator + (const DateTime &rhs) const noexcept;
    bool operator < (const DateTime &rhs) const noexcept;
    bool operator > (const DateTime &rhs) const noexcept;
    DateTime & operator = (const DateTime &rhs) noexcept;
    bool operator == (const DateTime &rhs) const noexcept;

    std::string to_string(const std::string &format = "%F", const int maxSize = 1024) const noexcept;

    template<typename OStream>
    friend OStream& operator << (OStream &os, const DateTime &rhs)
    {
        return os << rhs.to_nsecs_since_epoch();
    }

private:
    uint64_t nsec_;
    int symbol_ = 1;
};// class DateTime

class Rate
{
public:
    Rate() noexcept;
    explicit Rate(const uint64_t &rate) noexcept;
    /**
     * @brief wait for loop comming
     */
    void wait() noexcept;
    /**
     * @brief set rate
     */
    void set_rate(const uint64_t &rate) noexcept;
private:
    DateTime last_time_;
    DateTime inter_time_;
};
} // namespace coin
