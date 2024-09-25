/**
 * @file utils.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <functional>
#include <cxxabi.h>

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/ostream.h>

namespace coin
{
/**
 * @brief init system
 * 
 * @param argc count of arguments
 * @param argv list of arguments
 */
void init(int argc, char *argv[]);
/**
 * @brief check is still working
 * 
 * @return true 
 * @return false 
 */
bool ok();
/**
 * @brief shutdown system
 * when shutdown system, ok() will return false
 * 
 */
void shutdown();
/**
 * @brief execute spin once
 * 
 */
void spin_once();

namespace __inner
{
void register_init(const std::function<void(int, char *[])>& func);
void register_ok(const std::function<bool()>& func);
void register_exit(const std::function<void()>& func);
void register_spin_once(const std::function<void()>& func);
}

namespace utils
{
void setup();
bool ok_();
void exit();
}

class Print
{
friend void init(int argc, char *argv[]);
public:
template<typename S, typename... Args>
static inline void info(const S& format_str, Args&&... args)
{
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "[INFO] ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
};
template<typename S, typename... Args>
static inline void error(const S& format_str, Args&&... args)
{
    fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "[ERROR] ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
}
template<typename S, typename... Args>
static inline void warn(const S& format_str, Args&&... args)
{
    fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "[WARN] ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
}

template<typename S, typename... Args>
static inline void debug(const S& format_str, Args&&... args)
{
    // if(not is_enable_debug) return;
    fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
};
template<typename S, typename... Args>
static inline void debug_yes(const S& format_str, Args&&... args)
{
    if(not is_enable_debug) return;
    fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ✅ ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
};
template<typename S, typename... Args>
static inline void debug_no(const S& format_str, Args&&... args)
{
    if(not is_enable_debug) return;
    fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ❌ ");
    fmt::print(fmt::runtime(format_str), args...);
    std::putc('\n', stdout);
};

class tab_debuger
{
public:
    tab_debuger() = default;
    ~tab_debuger() = default;

    class debuger_guard
    {
    public:
        debuger_guard(tab_debuger& td) : td_(td) { td_.level_ += 1; }
        ~debuger_guard() { td_.level_ -= 1; }
    private:
        tab_debuger& td_;
    };
    friend class debuger_guard;

    template<typename S, typename... Args>
    inline void print(const S& format_str, Args&&... args)
    {
        if(not is_enable_debug) return;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
        fmt::print("{}", std::string(level_ * 2, ' '));
        fmt::print(fmt::runtime(format_str), args...);
        std::putc('\n', stdout);
    }

    template<typename S, typename... Args>
    inline void yes(const S& format_str, Args&&... args)
    {
        if(not is_enable_debug) return;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
        fmt::print("{}{} ", std::string(level_ * 2, ' '), "✅");
        fmt::print(fmt::runtime(format_str), args...);
        std::putc('\n', stdout);
    }

    template<typename S, typename... Args>
    inline void no(const S& format_str, Args&&... args)
    {
        if(not is_enable_debug) return;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
        fmt::print("{}{} ", std::string(level_ * 2, ' '), "❌");
        fmt::print(fmt::runtime(format_str), args...);
        std::putc('\n', stdout);
    }
    template<typename S, typename... Args>
    inline void launch(const S& format_str, Args&&... args)
    {
        if(not is_enable_debug) return;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
        fmt::print("{}{} ", std::string(level_ * 2, ' '), "🚀");
        fmt::print(fmt::runtime(format_str), args...);
        std::putc('\n', stdout);
    }

    template<typename S, typename... Args>
    inline void finished(const S& format_str, Args&&... args)
    {
        if(not is_enable_debug) return;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::blue), "[DEBUG] ");
        fmt::print("{}{} ", std::string(level_ * 2, ' '), "😁");
        fmt::print(fmt::runtime(format_str), args...);
        std::putc('\n', stdout);
    }

private:
    int level_ = 0;
};

private:
    static bool is_enable_debug;
};
} // namespace coin
