/**
 * @file utils.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "coin-commons/utils/utils.hpp"
#include <unistd.h>
#include <signal.h>
#include <memory>
#include <deque>
#include <mutex>
#include <sys/time.h>
#include <sys/file.h>
#include "utils.hpp"

static bool g_ok_ = true;

extern "C" {
static void handle_ctrl_c(int signum) {
    coin::Print::info("Ctrl+C received. Exiting...\n");
    g_ok_ = false;
}
}

namespace coin
{
bool Print::is_enable_debug = false;

namespace __inner
{
static std::mutex& g_init_funcs_mutex_()
{
    static std::mutex mutex_;
    return mutex_;
}
static std::deque< std::function<void(int, char*[])> >& g_init_funcs_()
{
    static auto funcs_ = std::make_shared< std::deque< std::function<void(int, char*[])> > >();
    return *funcs_;
}
static std::mutex& g_ok_funcs_mutex_()
{
    static std::mutex mutex_;
    return mutex_;
}
static std::deque< std::function<bool()> >& g_ok_funcs_()
{
    static auto funcs_ = std::make_shared< std::deque< std::function<bool()> > >();
    return *funcs_;
}
static std::mutex& g_exit_funcs_mutex_()
{
    static std::mutex mutex_;
    return mutex_;
}
static std::deque< std::function<void()> >& g_exit_funcs_()
{
    static auto funcs_ = std::make_shared< std::deque< std::function<void()> > >();
    return *funcs_;
}
static std::mutex& g_spin_funcs_mutex_()
{
    static std::mutex mutex_;
    return mutex_;
}
static std::deque< std::function<void()> >& g_spin_funcs_()
{
    static auto funcs_ = std::make_shared< std::deque< std::function<void()> > >();
    return *funcs_;
}
void register_init(const std::function<void(int, char *[])>& func)
{
    if(not func) return;
    {
        std::lock_guard<std::mutex> lock(g_init_funcs_mutex_());
        g_init_funcs_().push_back(func);
    }
}
void register_ok(const std::function<bool()>& func)
{
    if(not func) return;
    {
        std::lock_guard<std::mutex> lock(g_ok_funcs_mutex_());
        g_ok_funcs_().push_back(func);
    }
}
void register_exit(const std::function<void()>& func)
{
    if(not func) return;
    {
        std::lock_guard<std::mutex> lock(g_exit_funcs_mutex_());
        g_exit_funcs_().push_back(func);
    }
}
void register_spin_once(const std::function<void()>& func)
{
    if(not func) return;
    {
        std::lock_guard<std::mutex> lock(g_spin_funcs_mutex_());
        g_spin_funcs_().push_back(func);
    }
}
}

void init(int argc, char *argv[])
{
    {
        // invoke all init functions
        std::lock_guard<std::mutex> lock(__inner::g_init_funcs_mutex_());
        for(auto& func : __inner::g_init_funcs_())
        {
            func(argc, argv);
        }
    }
    // check debug setting
    char* enable_debug = getenv("COIN_ENABLE_DEBUG");
    if(enable_debug && std::string(enable_debug) == "ON")
    {
        Print::is_enable_debug = true;
        coin::Print::debug("enable debug output.");
    }
    // // regist sig int signal
    // signal(SIGINT, handle_ctrl_c);
}
bool ok()
{
    if(not g_ok_)
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(__inner::g_ok_funcs_mutex_());
        for(auto& func : __inner::g_ok_funcs_())
        {
            return false;
        }
    }
    return true;
}
void shutdown()
{
    {
        std::lock_guard<std::mutex> lock(__inner::g_exit_funcs_mutex_());
        for(auto& func : __inner::g_exit_funcs_())
        {
            func();
        }
    }
    g_ok_ = false;
}
void spin_once()
{
    std::lock_guard<std::mutex> lock(__inner::g_spin_funcs_mutex_());
    for(auto& func : __inner::g_spin_funcs_())
    {
        func();
    }
}
} // namespace coin
