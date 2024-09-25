/**
 * @file shm_memory.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <cstdio>
#include <coin-commons/utils/utils.hpp>
#include <coin-data/local/impl/ipcs.hpp>
#include <coin-data/local/impl/buddy.hpp>
#include <utility>
#include <functional>

#include <sys/types.h>
#include <signal.h>

namespace coin::data::__inner
{
class ShmMemory
{
public:
    constexpr static std::size_t PAGE_SIZE = 4096;

public:
    ShmMemory(const std::tuple<const std::string, const std::size_t, const void*>& ctl_info, const std::tuple<const std::string, const std::size_t, const void*>& data_info);
    ShmMemory(const std::pair<const std::string, const std::size_t>& ctl_info, const std::pair<const std::string, const std::size_t>& data_info);
    ShmMemory(const ShmMemory&) = delete;
    ShmMemory& operator = (const ShmMemory&) = delete;
    ~ShmMemory();

    void initialized();
    void destory();

    void* malloc(const std::size_t size);
    bool free(void* mem);

    const bool is_busy() const;

    void* const ctl_addr() const;
    void* const data_addr() const;

    template<typename T>
    class ProcessLockArea;
    #pragma pack(4)
    class ProcessMutex
    {
        friend class ProcessLockArea<ProcessMutex>;
    public:
        ProcessMutex();
        // ProcessMutex(ProcessMutex&& rhs);
        ~ProcessMutex();

        // DO NOT COPY
        ProcessMutex(const ProcessMutex&) = delete;
        ProcessMutex& operator = (const ProcessMutex&) = delete;

        // DO NOT MOVE
        ProcessMutex(ProcessMutex&&) = delete;
        ProcessMutex& operator = (ProcessMutex&&) = delete;

        bool lock();
        bool unlock();

        int timedlock(int ms);

        inline pid_t holder() const { return holder_pid_; }

    private:
        pthread_mutex_t mutex_;
        pid_t holder_pid_;
        const pid_t owener_pid_;
        bool is_died_;
    };

    class ProcessRWMutex
    {
    public:
        ProcessRWMutex();
        ~ProcessRWMutex();

        bool rlock();
        bool wlock();
        bool unlock();

    private:
        pthread_rwlock_t rwlock_;
    };
    #pragma pack()

    template<typename MutexT>
    class ProcessLockGuard
    {
        friend ProcessMutex;
    public:
        ProcessLockGuard(MutexT& m) : m_(m)
        { m_.lock(); }

        ProcessLockGuard(MutexT& m, int ms) : m_(m)
        { m_.timedlock(ms); }

        ~ProcessLockGuard()
        { m_.unlock(); }

    private:
        MutexT& m_;
    }; // ProcessLockGuard

    template<typename MutexT>
    class ProcessRLockGuard
    {
    public:
        ProcessRLockGuard(MutexT& m) : m_(m)
        { m_.rlock(); }

        ~ProcessRLockGuard()
        { m_.unlock(); }

    private:
        MutexT& m_;
    }; // ProcessRLockGuard

    template<typename MutexT>
    class ProcessWLockGuard
    {
    public:
        ProcessWLockGuard(MutexT& m) : m_(m)
        { m_.wlock(); }

        ~ProcessWLockGuard()
        { m_.unlock(); }

    private:
        MutexT& m_;
    }; // ProcessWLockGuard

    template<typename MutexT>
    class ProcessLockArea
    {
    public:
        ProcessLockArea(MutexT& m,
            const std::function<void()>& locked_func,
            const std::function<void()>& failed_func = nullptr, size_t timeout = 1000 /*ms*/)
          : m_(m)
        {
            if(not locked_func) return;

            // try to lock
            while(0 != m_.timedlock(timeout))
            {
                coin::Print::warn("lock {} timeout", m_.holder());
                // lock required timeout, check holder exist or not
                if(m_.holder() != 0 and kill(m_.holder(), 0) == 0)
                {
                    coin::Print::debug("lock {} still working, continue wait for it.", m_.holder());
                    continue;
                }
                else if(kill(m_.owener_pid_, 0) != 0)
                {
                    coin::Print::debug("lock {} owener is died, forgive lock.", m_.holder());
                    m_.is_died_ = true;
                    m_.unlock();
                    continue;
                }
                else
                {
                    coin::Print::debug("holder {} is died, unlock.", m_.holder());
                    m_.unlock();
                    continue;
                }
            }

            if(not m_.is_died_)
            {
                locked_func();
            }
            else
            {
                if(failed_func)
                {
                    coin::Print::debug("{} call failed_func()", m_.holder());
                    failed_func();
                }
            }

            m_.unlock();
        }
        ~ProcessLockArea() = default;

        // DO NOT COPY
        ProcessLockArea(const ProcessLockArea&) = delete;
        ProcessLockArea& operator = (const ProcessLockArea&) = delete;
        // DO NOT MOVE
        ProcessLockArea(ProcessLockArea&&) = delete;
        ProcessLockArea& operator = (ProcessLockArea&&) = delete;
    private:
        MutexT& m_;
    };

private:
    const std::string key_file_;

    std::unique_ptr<Shm> shm_info_;
    std::unique_ptr<Shm> shm_data_info_;
};

} // namespace coin::data::__inner
