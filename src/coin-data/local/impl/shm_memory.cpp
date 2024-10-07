/**
 * @file shm_memory.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <coin-data/local/impl/shm_memory.hpp>
#include <coin-commons/utils/utils.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

namespace coin::data::__inner
{
ShmMemory::ShmMemory(
        const std::tuple<const std::string, const std::size_t, const void*>& ctl_info,
        const std::tuple<const std::string, const std::size_t, const void*>& data_info
    )
{
    // create shm memory
    shm_info_      = std::make_unique<Shm>(std::get<0>(ctl_info),  std::get<1>(ctl_info),  std::get<2>(ctl_info));
    shm_data_info_ = std::make_unique<Shm>(std::get<0>(data_info), std::get<1>(data_info), std::get<2>(data_info));
}
ShmMemory::ShmMemory(
        const std::pair<const std::string, const std::size_t>& ctl_info,
        const std::pair<const std::string, const std::size_t>& data_info
    )
{
    // create shm memory
    shm_info_      = std::make_unique<Shm>(ctl_info.first,  ctl_info.second);
    shm_data_info_ = std::make_unique<Shm>(data_info.first, data_info.second);
}
ShmMemory::~ShmMemory()
{
}
void ShmMemory::initialized()
{
    // initilize buddy system
    if(0 > buddy_init_system(shm_info_->addr(), PAGE_SIZE))
    {
        coin::Print::error("sys info buddy init system failed.");
        exit(-1);
    }

    // attach data memory 
    if(0 > buddy_attch_memory((struct buddy_head_t* )shm_info_->addr(), shm_data_info_->addr(), shm_data_info_->size()))
    {
        coin::Print::error("buddy attach memory failed.");
        exit(-1);
    }
}
void ShmMemory::destory()
{
}
void *ShmMemory::malloc(const std::size_t size)
{
    return buddy_malloc((struct buddy_head_t* )shm_info_->addr(), size);
}
bool ShmMemory::free(void *mem)
{
    return (0 == buddy_free((struct buddy_head_t* )shm_info_->addr(), mem));
}
const bool ShmMemory::is_busy() const
{
    return (shm_info_->num_of_attach() > 1);
}

void* const ShmMemory::ctl_addr() const
{
    return shm_info_->addr();
}
void* const ShmMemory::data_addr() const
{
    return shm_data_info_->addr();
}
ShmMemory::ProcessMutex::ProcessMutex()
  : holder_pid_(0)
  , owener_pid_(getpid())
  , is_died_(false)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&mutex_, &attr);
}
ShmMemory::ProcessMutex::~ProcessMutex()
{
    if(0 != holder())
    {
        coin::Print::error("relase mutex failed: mutex is still using by: {}", holder());
        abort();
    }
    /**
     * @brief if pthread_mutex_destroy return EBUSY,
     *         it means mutex is used by other process,
     *         so we should wait it finish.
     *         just for destroy mutex safely, but it's not a good way.
     *         NOTE: reimplement it.
     */
    int r = 0;
    while((r = pthread_mutex_destroy(&mutex_)) == EBUSY)
    {
        coin::Print::debug("mutex is busy, using by: {}, owener: {}, ret: {}, {:X}", holder(), owener_pid_, r, (uint64_t)this);
        if(0 != holder())
        {
            coin::Print::error("destroy mutex failed: mutex is still using by: {}", holder());
            abort();
        }
        usleep(100);
    }
    if(r != 0)
    {
        coin::Print::error("mutex destroy failed({}): {} {}", r, holder(), owener_pid_);
        abort();
    }
}
bool ShmMemory::ProcessMutex::lock()
{
    if(0 != pthread_mutex_lock(&mutex_))
    {
        return false;
    }
    holder_pid_ = getpid();
    return true;
}
bool ShmMemory::ProcessMutex::unlock()
{
    if(holder_pid_ == 0)
    {
        coin::Print::error("unlock failed: holder_pid_ == 0");
        abort();
    }
    holder_pid_ = 0;
    int r = pthread_mutex_unlock(&mutex_);
    if(EINVAL == r)
    {
        coin::Print::error("unlock failed: EINVAL({})", r);
        abort();
        return false;
    }
    else if(0 != r)
    {
        coin::Print::error("unlock failed: {}({})", strerror(r), r);
        abort();
        return false;
    }
    return true;
}
int ShmMemory::ProcessMutex::timedlock(int ms)
{
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    uint64_t now = timeout.tv_sec * 1000 * 1000 * 1000 + timeout.tv_nsec + (ms * 1000);

    timeout.tv_sec = now / 1000 / 1000 / 1000;
    timeout.tv_nsec = now % (1000 * 1000 * 1000);
    int ret = pthread_mutex_timedlock(&mutex_, &timeout);
    if(ret == 0)
    {
        holder_pid_ = getpid();
    }
    else if(ret == ETIMEDOUT)
    {

    }
    else if(ret == EINVAL /* 22 */)
    {
        
    }
    else
    {
        coin::Print::error("{}({}), {}, {}.\n", strerror(ret), ret, timeout.tv_sec, timeout.tv_nsec);
        abort();
    }
    return ret;
}
ShmMemory::ProcessRWMutex::ProcessRWMutex()
{
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&rwlock_, &attr);
}
ShmMemory::ProcessRWMutex::~ProcessRWMutex()
{
    pthread_rwlock_destroy(&rwlock_);
}
bool ShmMemory::ProcessRWMutex::rlock()
{
    if(0 != pthread_rwlock_rdlock(&rwlock_))
    {
        return false;
    }
    return true;
}
bool ShmMemory::ProcessRWMutex::wlock()
{
    if(0 != pthread_rwlock_wrlock(&rwlock_))
    {
        return false;
    }
    return true;
}
bool ShmMemory::ProcessRWMutex::unlock()
{
    if(0 != pthread_rwlock_unlock(&rwlock_))
    {
        return false;
    }
    return true;
}
} // namespace coin::data::__inner
