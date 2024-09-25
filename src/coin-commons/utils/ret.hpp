/**
 * @file ret.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <string>

namespace coin
{

template<typename T>
struct Ret
{

public:
    Ret()
      : value_()
      , ret_(false)
      , reason_("") {}
    Ret(const T& v, const bool& ret = false, const std::string& reason = "")
      : value_(v)
      , ret_(ret)
      , reason_(reason) {}

    Ret(const T& v, const std::string& reason = "")
      : value_(v)
      , ret_(false)
      , reason_(reason) {}


    Ret& operator = (const T& v)
    {
        value_ = v; ret_ = false; reason_ = ""; return *this;
    }
    Ret& operator = (const Ret& r)
    {
        value_ = r.value_; ret_ = r.ret_; ret_ = r.ret_; return *this;
    }

    bool operator () () { return ret_; }
    Ret& operator() (const T& v, const bool& ret = false, const std::string& reason = "")
    {
        value_ = v;
        ret_ = ret;
        reason_ += reason;
        return ret_;
    }
    Ret& operator() (const bool& ret, const std::string& reason = "")
    {
        ret_ = ret;
        reason_ += reason;
        return *this;
    }
    T& operator * () { return value_; }
    T& get() { return value_; }

    
    bool operator == (const T& v) const { return value_ == v; }
    bool operator == (const Ret& r) const
    {
        return value_ == r.value_ && ret_ == r.ret_;
    }

    // operation for reason
    std::string reason() { return reason_; }
    void clear_reason() { reason_.clear(); }
    Ret& operator << (const std::string& r)
    {
        reason_ += r;
        return *this;
    }

private:
    T value_;
    bool ret_ = false;
    std::string reason_;

};

template<>
struct Ret<bool>
{

public:
    Ret(const bool& v = false, const std::string& reason = "")
      : value_(v)
      , reason_(reason) {}

    Ret& operator = (const bool& v)
    {
        value_ = v;
        reason_ = "";
        return *this;
    }
    Ret& operator = (const Ret& r)
    {
        value_ = r.value_;
        reason_ = r.reason_;
        return *this;
    }

    operator bool() { return value_; }
    bool& operator () () { return value_; }
    Ret& operator () (const bool& v, const std::string& reason = "")
    {
        value_ = v;
        reason_ += reason;
        return *this;
    }
    bool& operator * () { return value_; }
    bool& get() { return value_; }
    bool operator == (const bool& v) const { return value_ == v; }
    bool operator == (const Ret& r) const
    {
        return value_ == r.value_;
    }
    // operation for reason
    std::string reason() { return reason_; }
    void clear_reason() { reason_.clear(); }
    Ret& operator << (const std::string& r)
    {
        reason_ += r;
        return *this;
    }

private:
    bool value_ = false;
    std::string reason_;
};

using RetBool   = Ret<bool>;
using RetInt    = Ret<int>;
using RetFloat  = Ret<float>;
using RetDouble = Ret<double>;
} // namespace coin
