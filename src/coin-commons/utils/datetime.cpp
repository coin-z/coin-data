/**
 * @file datetime.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "datetime.hpp"
#include <sys/time.h>
#include <cassert>
#include <unistd.h>
#include <chrono>

namespace coin
{
DateTime::DateTime() noexcept : nsec_(0)
{

}
DateTime::DateTime(const uint64_t &t) noexcept : nsec_(t)
{

}
DateTime::~DateTime() noexcept
{

}
DateTime DateTime::current_date_time()
{
    DateTime dt;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp =
        std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());

    auto tmp = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
    dt.nsec_ = tmp.count();
    dt.symbol_ = 1;

    return std::move(dt);
}
uint64_t DateTime::to_secs_since_epoch() const noexcept
{
    return nsec_ / 1000 / 1000 / 1000;
}
uint64_t DateTime::to_msecs_since_epoch() const noexcept
{
    return nsec_ / 1000 / 1000;
}
uint64_t DateTime::to_usecs_since_epoch() const noexcept
{
    return nsec_ / 1000;
}
uint64_t DateTime::to_nsecs_since_epoch() const noexcept
{
    return nsec_;
}
struct tm DateTime::to_tm() const noexcept
{
    struct tm _tm;
    time_t _time = nsec_ / 1000000000 ;
    localtime_r(&_time, &_tm);
    return _tm;
}
void DateTime::set_date_time(uint64_t timestamp) noexcept
{
    nsec_ = timestamp;
    symbol_ = 1;
}
void DateTime::set_date_time_sec(uint64_t timestamp) noexcept
{
    nsec_ = timestamp*1000*1000*1000;
    symbol_ = 1;
}
void DateTime::set_date_time_msec(uint64_t timestamp) noexcept
{
    nsec_ = timestamp*1000*1000;
    symbol_ = 1;
}
void DateTime::set_date_time_usec(uint64_t timestamp) noexcept
{
    nsec_ = timestamp*1000;
    symbol_ = 1;
}
void DateTime::set_date_time_nsec(uint64_t timestamp) noexcept
{
    nsec_ = timestamp;
    symbol_ = 1;
}
DateTime DateTime::operator - (const DateTime &rhs) const noexcept
{
    DateTime val;
    if(this->nsec_ > rhs.nsec_)
    {
        val.nsec_ = this->nsec_ - rhs.nsec_;
        val.symbol_ = 1;
    }
    else
    {
        val.nsec_ = rhs.nsec_ - this->nsec_;
        val.symbol_ = -1;
    }
    return val;
}
DateTime DateTime::operator + (const DateTime &rhs) const noexcept
{
    DateTime val;
    if(this->symbol_ != rhs.symbol_)
    {
        if(this->nsec_ > rhs.nsec_)
        {
            val.nsec_ = this->nsec_ - rhs.nsec_;
            val.symbol_ = this->symbol_;
        }
        else
        {
            val.nsec_ = rhs.nsec_ - this->nsec_;
            val.symbol_ = rhs.symbol_;
        }
    }
    else
    {
        val.nsec_ = this->nsec_ + rhs.nsec_;
        val.symbol_ = this->symbol_;
    }

    return val;
}
bool DateTime::operator < (const DateTime &rhs) const noexcept
{
    return this->nsec_ < rhs.nsec_;
}
bool DateTime::operator > (const DateTime &rhs) const noexcept
{
    return this->nsec_ > rhs.nsec_;
}
DateTime & DateTime::operator = (const DateTime &rhs) noexcept
{
    this->nsec_ = rhs.nsec_;
    this->symbol_ = rhs.symbol_;
    return *this;
}
bool DateTime::operator == (const DateTime &rhs) const noexcept
{
    return (this->nsec_ == rhs.nsec_ && this->symbol_ == rhs.symbol_);
}
uint64_t DateTime::sec() const noexcept
{
    return (nsec_ / 1000000000);
}
uint64_t DateTime::msec()  const noexcept
{
    return (nsec_ / 1000000 % 1000);
}
uint64_t DateTime::usec() const noexcept
{
    return (nsec_ / 1000 % 1000);
}
uint64_t DateTime::nsec() const noexcept
{
    return (nsec_ % 1000);
}
uint64_t DateTime::value() const noexcept
{
    return nsec_;
}
int DateTime::symbol() const noexcept
{
    return symbol_;
}
std::string DateTime::to_string(const std::string &format, const int maxSize) const noexcept
{
    time_t tt = nsec_ / 1000000000;
    tm *t = localtime(&tt);

    std::string str_time;
    str_time.resize(maxSize);
    int _s = strftime(str_time.data(), maxSize, format.c_str(), t);
    str_time.resize(_s);

    return std::move(str_time);
}
Rate::Rate() noexcept : last_time_(0), inter_time_(0)
{

}
Rate::Rate(const uint64_t &rate) noexcept : last_time_(0), inter_time_((uint64_t)((1000000000.0 / (double)rate)))
{
    assert(rate <= 1000);// 最大频率为 1000hz
}
void Rate::set_rate(const uint64_t &rate) noexcept
{
    inter_time_ = DateTime((uint64_t)((1000000000.0 / (double)rate)));
    last_time_ = 0;
}
void Rate::wait() noexcept
{
    if(inter_time_.to_usecs_since_epoch() == 0)
    {
        return;
    }
    if(last_time_.value() == 0)
    {
        last_time_ = DateTime::current_date_time();
        return;
    }

    auto dt = DateTime::current_date_time() - last_time_;
    {
        if(inter_time_.to_usecs_since_epoch() > dt.to_usecs_since_epoch())
        {
            usleep(inter_time_.to_usecs_since_epoch() - dt.to_usecs_since_epoch());
        }
    }
    last_time_ = DateTime::current_date_time();
}
} // namespace coin
