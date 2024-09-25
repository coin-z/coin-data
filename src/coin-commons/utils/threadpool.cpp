/**
 * @file threadpool.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "coin-commons/utils/threadpool.hpp"
#include <unistd.h>
#include "threadpool.hpp"


#define WORKER_WAITTING_STATUS_TIMEOUT_CHECK (10000)

namespace coin
{

ThreadPool::ThreadPool(const size_t &size) : reserve_(size)
  , size_(0)
  , is_working_(true)
{
    task_watcher_worker_ = std::make_shared<std::thread>(std::bind(&ThreadPool::task_watcher_, this));
}

ThreadPool::~ThreadPool()
{
    is_working_ = false;
    for( auto &worker : workers_ )
    {
        // 唤醒工作线程
        std::unique_lock<std::mutex> lock(worker->mtx);
        worker->cv.notify_one();
    }
    for(auto &worker : workers_ )
    {
        worker->thread->join();
    }

    if(task_watcher_worker_)
    {
        task_watcher_worker_->join();
        task_watcher_worker_.reset();
    }
}

void ThreadPool::task_watcher_()
{
    while(is_working_)
    {
        sleep(1);
        {
            {
                std::lock_guard<std::mutex> lock(worker_lock_);
                for(const auto & itor : workers_)
                {
                    if(itor->status == Worker::WorkStatus::WAITING)
                    {
                        const auto activeTime = itor->active_time.load();
                        const auto now = coin::DateTime::current_date_time().to_usecs_since_epoch();
                        const size_t interval =  now - activeTime;
                        if(itor->tasks.size() > 0 && interval > WORKER_WAITTING_STATUS_TIMEOUT_CHECK)
                        {
                            coin::Print::warn("woker: {} is stopped for {}, but it still has {} task(s) unfinished.", itor->idx, interval, itor->tasks.size());
                        }
                    }
                }
            }
        }
    }
}

void ThreadPool::exec()
{
    // for( size_t i = 0; i < size_; i++ )
    // {
    //     workers_.emplace_back(
    //         std::make_shared<std::thread>(std::thread(&ThreadPool::run_, this))
    //         );
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////

ThreadBindPool::ThreadBindPool(const size_t &size) : ThreadPool(size)
{

}

ThreadBindPool::~ThreadBindPool()
{

}

void ThreadBindPool::add_task_to(ThreadPool::TaskPtr task, const size_t &index)
{

    // 检查 index 合法性
    if(index > reserve_)
    {
        coin::Print::error("add task <{}> is invalid", index);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(worker_lock_);
        while(workers_.size() <= (index))
        {
            workers_.emplace_back(std::make_shared<ThreadPool::Worker>());
        }
        size_ = workers_.size();

        auto& worker = workers_[index];

        task->add_time = coin::DateTime::current_date_time().to_usecs_since_epoch();
        worker->tasks.push_back(task);
        worker->idx = index;

        // 检查 Worker 工作时间是否过长
        auto now = coin::DateTime::current_date_time().to_usecs_since_epoch();
        auto start_time = worker->start_time.load();
        if( worker->status.load() == ThreadPool::Worker::WorkStatus::WORKING and
            start_time != 0 and (now - start_time) > WORKER_WAITTING_STATUS_TIMEOUT_CHECK)
        {
            coin::Print::warn("worker <{}> has worked for a long time, start: {}, interval: {}us.", index, start_time, now - start_time);
        }

        if(worker->thread != nullptr)
        {
            // 工作线程已经启动，
            auto is_work = worker->is_running.load();
            if(not is_work)
            {
                worker->is_running.store(true);
                worker->thread->join();
                worker->thread = std::make_shared<std::thread>(std::thread(&ThreadBindPool::run_, this, worker));
            }
        }
        else
        {
            worker->is_running.store(true);
            worker->thread = std::make_shared<std::thread>(std::thread(&ThreadBindPool::run_, this, worker));
        }
        {
            // 唤醒工作线程
            std::unique_lock<std::mutex> lock(worker->mtx);
            worker->cv.notify_one();
        }

    }
}

void ThreadBindPool::exec()
{
    // for( size_t i = 0; i < size_; i++ )
    // {
    //     workers_.emplace_back(
    //         std::make_shared<std::thread>(std::thread(&ThreadBindPool::run_, this, i))
    //         );
    // }
}

void ThreadBindPool::spin_once()
{
    std::lock_guard<std::mutex> lock(worker_lock_);
    for(const auto & itor : workers_)
    {
        if(itor->status == Worker::WorkStatus::WAITING)
        {
            if(itor->tasks.size() > 0)
            {
                // 唤醒工作线程
                std::unique_lock<std::mutex> lock(itor->mtx);
                itor->cv.notify_one();
            }
        }
    }
}

void ThreadBindPool::run_(std::shared_ptr< Worker > worker)
{
    const std::string selfName = std::to_string((uint64_t)this) + "-worker-" + std::to_string(worker->idx);
    pthread_setname_np(pthread_self(), selfName.c_str());

    while(is_working_)
    {
        auto now = coin::DateTime::current_date_time().to_usecs_since_epoch();
        worker->active_time.store(now);
        TaskPtr task;
        if(size_ != 0)
        {
            while(true && is_working_)
            {
                {
                    if(worker->tasks.size() == 0)
                    {
                        break;
                    }
                    worker->tasks.pop_front(task);
                }
                if(task && task->task) 
                {
                    // worker 进入工作状态，记录当前任务启动时间
                    auto now = coin::DateTime::current_date_time().to_usecs_since_epoch();
                    worker->start_time.store(now);
                    worker->status.store(Worker::WorkStatus::WORKING);
                    task->invoke_time = coin::DateTime::current_date_time().to_usecs_since_epoch();
                    (*(task->task))();
                    task->finished_time = coin::DateTime::current_date_time().to_usecs_since_epoch();
                    // 任务结束，改变状态
                    worker->status.store(Worker::WorkStatus::IDLE);
                    task.reset();
                }
            }
        }

        if(worker->tasks.size() == 0)
        {
            // workers_[idx]->is_running.store(false);
            // break;
            if(not is_working_) break;
            // 等待条件变量唤醒
            worker->status.store(Worker::WorkStatus::WAITING);
            std::unique_lock<std::mutex> lock(worker->mtx);
            worker->cv.wait(lock, [this, &worker]{
                return (worker->tasks.size() > 0) || not is_working_;
            });
            // std::this_thread::sleep_for(std::chrono::microseconds(1000));
            worker->status.store(Worker::WorkStatus::IDLE);
        }

    }
}
} // namespace coin
