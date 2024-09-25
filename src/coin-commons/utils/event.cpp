/**
 * @file event.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "utils/event.hpp"
#include "event.hpp"
#include <coin-commons/utils/threadpool.hpp>

namespace coin
{
EventPool::EventPool() : thread_pool_( std::make_shared<coin::ThreadBindPool>(6) )
{
}
EventPool::~EventPool()
{
}
void EventPool::run_and_clear_all_ready_task()
{
    std::lock_guard<std::mutex> lock(ready_task_deque_mutex_);
    for(auto it = ready_task_deque_.begin(); it != ready_task_deque_.end(); ++it)
    {
        // asio::post(*thread_pool_, std::bind(&TaskBase::invoke, (*it).get()));
        thread_pool_->add_task_to(0, std::bind(&TaskBase::invoke, (*it).get()));
    }
    ready_task_deque_.clear();
}
void EventPool::clear_all_ready_task()
{
    std::lock_guard<std::mutex> lock(ready_task_deque_mutex_);
    ready_task_deque_.clear();
}
void EventPool::spin_once()
{
    std::lock_guard<std::mutex> lock(event_deque_mutex_);
    for(auto it = event_deque_.begin(); it != event_deque_.end(); ++it)
    {
        if(it->first->operator bool())
        {
            std::lock_guard<std::mutex> ready_task_lock(ready_task_deque_mutex_);
            ready_task_deque_.push_back(it->second);
        }
    }
}
} // namespace coin
