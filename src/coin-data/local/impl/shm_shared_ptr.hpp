/**
 * @file shm_shared_ptr.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <atomic>

namespace coin::data::__inner
{
template<typename T, typename AllocT, typename MutexT>
class ShmSharedPtr
{
    using counter_t = std::atomic< int >;
    struct Private{};
public:
    ShmSharedPtr() = default;

    ShmSharedPtr(T* raw) noexcept : raw_ptr_(raw)
    {
        typename AllocT::template rebind< MutexT >::other mutex_alloc;
        mutex_ = mutex_alloc.allocate(1);
        mutex_alloc.construct(mutex_);

        typename AllocT::template rebind< counter_t >::other cnt_alloc;
        used_counter_ = cnt_alloc.allocate(1);
        cnt_alloc.construct(used_counter_);

        increace_(this->used_counter_);
    }

    ~ShmSharedPtr()
    {
        if(not this->raw_ptr_) { return; }
        lock_(this);
        this->decreace_();
        if(mutex_) { mutex_->unlock(); }
    }

    ShmSharedPtr(ShmSharedPtr&& rhs) noexcept
    {
        if(not rhs.raw_ptr_) { return; }
        if(this == &rhs) { return; }

        if(this->raw_ptr_)
        {
            lock_(this);
            this->decreace_();
            if(mutex_) this->mutex_->unlock();
        }
        lock_(&rhs);
        this->raw_ptr_ = rhs.raw_ptr_;
        this->used_counter_ = rhs.used_counter_;
        this->mutex_ = rhs.mutex_;

        increace_(this->used_counter_);

        rhs.decreace_();
        rhs.raw_ptr_ = nullptr;
        rhs.used_counter_ = nullptr;
        rhs.mutex_ = nullptr;
        this->mutex_->unlock();
    }

    ShmSharedPtr(const ShmSharedPtr& rhs) noexcept
    {
        if(not rhs.raw_ptr_) { return; }
        if(this == &rhs) { return; }
        if(this->mutex_)
        {
            lock_(this);
            this->decreace_();
            if(this->mutex_) this->mutex_->unlock();
        }
        lock_(&rhs);
        this->raw_ptr_ = rhs.raw_ptr_;
        this->used_counter_ = rhs.used_counter_;
        this->mutex_ = rhs.mutex_;

        increace_(this->used_counter_);

        if(rhs.mutex_) rhs.mutex_->unlock();
    }

    ShmSharedPtr& operator = (const ShmSharedPtr& rhs) noexcept
    {
        if(not rhs.raw_ptr_) { return *this; }
        if(this == &rhs) { return *this; }

        if(this->raw_ptr_)
        {
            lock_(this);
            this->decreace_();
            if(this->mutex_) this->mutex_->unlock();
        }
        lock_(&rhs);
        this->raw_ptr_ = rhs.raw_ptr_;
        this->used_counter_ = rhs.used_counter_;
        this->mutex_ = rhs.mutex_;

        increace_(this->used_counter_);

        rhs.mutex_->unlock();
        return *this;
    }

    ShmSharedPtr& operator = (ShmSharedPtr&& rhs) noexcept
    {
        if(not rhs.raw_ptr_) { return *this; }
        if(this == &rhs) { return *this; }

        if(this->raw_ptr_)
        {
            lock_(this);
            this->decreace_();
            if(this->mutex_) this->mutex_->unlock();
        }

        lock_(&rhs);
        this->raw_ptr_ = rhs.raw_ptr_;
        this->used_counter_ = rhs.used_counter_;
        this->mutex_ = rhs.mutex_;

        increace_(this->used_counter_);

        rhs.decreace_();
        rhs.raw_ptr_ = nullptr;
        rhs.used_counter_ = nullptr;
        rhs.mutex_ = nullptr;
        this->mutex_->unlock();

        return *this;
    }

    void reset()
    {
        if(not raw_ptr_) { return; }
        if(this->mutex_)
        {
            lock_(this);
            this->decreace_();

            this->raw_ptr_ = nullptr;
            this->used_counter_ = nullptr;
            if(this->mutex_) this->mutex_->unlock();
        }
        this->mutex_ = nullptr;
    }

    inline T* operator -> () noexcept { return raw_ptr_; }
    inline const T* operator -> () const noexcept { return raw_ptr_; }

    inline const T* get() const noexcept { return raw_ptr_; }
    inline T* get() noexcept { return raw_ptr_; }

    inline T& operator* () noexcept { return *raw_ptr_; }
    inline const T& operator* () const noexcept { return *raw_ptr_; }

    operator bool () const noexcept { return raw_ptr_ != nullptr; }

    bool operator == (const std::nullptr_t)
    {
        return raw_ptr_ == nullptr;
    }

    bool operator != (const std::nullptr_t)
    {
        return raw_ptr_ != nullptr;
    }

    inline size_t used_count() const noexcept
    {
        if(not raw_ptr_) { return 0; }
        if(not used_counter_) { return 0; }
        return used_counter_->load();
    }
private:
    T* raw_ptr_ = nullptr;
    counter_t* used_counter_ = nullptr;
    MutexT *mutex_ = nullptr;
    inline static pid_t self_pid_ = getpid();

    static void lock_(const ShmSharedPtr* ptr) noexcept
    {
        if(ptr && ptr->mutex_)
        {
            while(ptr->mutex_->timedlock(100) != 0)
            {
                // 检查 tmux holder 是否存在
                if(kill(ptr->mutex_->holder(), 0) == 0)
                {
                    continue;
                }
                else
                {
                    ptr->mutex_->unlock();
                    continue;
                }
            }
        }
    }

    void decreace_() noexcept
    {
        if(this->used_counter_)
        {
            used_counter_->fetch_sub(1);

            if(this->used_counter_->load() == 0)
            {
                AllocT data_alloc;
                data_alloc.destroy(this->raw_ptr_);
                data_alloc.deallocate(this->raw_ptr_, 1);

                typename AllocT::template rebind< counter_t >::other cnt_alloc;
                cnt_alloc.destroy(this->used_counter_);
                cnt_alloc.deallocate(this->used_counter_, 1);

                // 解锁
                this->mutex_->unlock();
                typename AllocT::template rebind< MutexT >::other mutex_alloc;
                mutex_alloc.destroy(this->mutex_);
                mutex_alloc.deallocate(this->mutex_, 1);
                this->mutex_ = nullptr;
            }
            this->used_counter_ = nullptr;
            this->raw_ptr_ = nullptr;
        }
    }

    static void increace_(counter_t* used_cnt) noexcept
    {
        if(not used_cnt) return;
        used_cnt->fetch_add(1);
    }

};

template<typename T, typename AllocT, typename MutexT, typename... ArgsT>
ShmSharedPtr<T, AllocT, MutexT> make_shared_obj(ArgsT&&... args)
{
    AllocT alloc;
    T* ptr = alloc.allocate(1);
    alloc.construct(ptr, std::forward<ArgsT>(args)...);
    return ShmSharedPtr<T, AllocT, MutexT>(ptr);
};

} // namespace coin::data::__inner


