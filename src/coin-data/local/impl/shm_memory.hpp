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
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

namespace coin::data::__inner
{
class ShmMemory
{
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

    static std::size_t page_size();

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

        int timedlock(size_t ms);

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
            const std::function<void()>& locked_func, size_t timeout = 1000 /*ms*/)
          : m_(m)
        {
            if(not locked_func) return;

            // try to lock
            /** @todo NOTE: how to handle timeout? when holder process is died,
                     how to unlock it?
            */
            int r = 0;
            while(0 != (r = m_.timedlock(timeout)))
            {
                coin::Print::warn("lock {} timeout", m_.holder());
                // lock required timeout, check holder exist or not
                if(m_.holder() != 0 and kill(m_.holder(), 0) == 0)
                {
                    coin::Print::debug("lock {}({:X}) still working, continue wait for it.", m_.holder(), (uint64_t)&m_);
                    continue;
                }
                else
                {
                    // lock holder is died, fix lock here.
                    pthread_mutex_consistent(&m_.mutex_);
                }
                if(kill(m_.owener_pid_, 0) != 0)
                {
                    coin::Print::debug("lock self({}) holder({}) owener({}) owener is died, forgive lock.", getpid(), m_.holder(), m_.owener_pid_);
                    m_.is_died_ = true;
                    abort();
                    break;
                }
                else
                {
                    if(m_.owener_pid_ == getpid())
                    {
                        m_.unlock();
                    }
                    // forgice this resource, wait for main process unlock it.
                    coin::Print::debug("lock {:X}, holder: {} owener is working, wait for it.", (uint64_t)this, m_.holder());
                    return;
                }
            }

            if(not m_.is_died_)
            {
                locked_func();
                m_.unlock();
            }
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
