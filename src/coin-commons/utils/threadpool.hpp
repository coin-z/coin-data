/**
 * @file threadpool.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#pragma once

#include <deque>
#include <future>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <vector>

#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/ret.hpp>
#include <coin-commons/utils/datetime.hpp>


template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue(int capacity) : buffer(capacity), capacity(capacity), head(0), tail(0) {}

    bool push_back(const T& value) {
        int currentTail = tail.load(std::memory_order_relaxed);
        int nextTail = (currentTail + 1) % capacity;

        if (nextTail == head.load(std::memory_order_acquire)) {
            return false; // 队列已满
        }

        buffer[currentTail] = value;
        tail.store(nextTail, std::memory_order_release);
        return true;
    }

    bool pop_front(T& value) {
        int currentHead = head.load(std::memory_order_relaxed);

        if (currentHead == tail.load(std::memory_order_acquire)) {
            return false; // 队列为空
        }

        value = buffer[currentHead];
        head.store((currentHead + 1) % capacity, std::memory_order_release);
        return true;
    }

    int size() {
        int currentTail = tail.load(std::memory_order_relaxed);
        int currentHead = head.load(std::memory_order_relaxed);
        return (currentTail - currentHead + capacity) % capacity;
    }

private:
    std::vector<T> buffer;
    int capacity;
    std::atomic<int> head;
    std::atomic<int> tail;
};


namespace coin
{

class ThreadPool
{
    struct TaskPackageInfo {
        using TaskT = std::packaged_task<coin::RetBool()>;

        std::shared_ptr< TaskT > task;
        uint64_t owener_id = 0;
        uint64_t add_time = 0;
        uint64_t invoke_time = 0;
        uint64_t finished_time = 0;

    };


public:

    using Task = TaskPackageInfo;
    using TaskPtr = std::shared_ptr< Task >;
    using TaskQueue = std::deque< TaskPtr >;
    using PriorityTaskQueue = std::vector<TaskQueue>;
    struct Worker {

        enum class WorkStatus {
            IDLE,
            WORKING,
            WAITING,
            STOP
        };

        std::shared_ptr< std::thread > thread;
        LockFreeQueue< ThreadPool::TaskPtr > tasks;
        std::atomic<bool> is_running;
        std::mutex mtx;
        std::condition_variable cv;

        std::atomic<size_t> active_time = 0;
        std::atomic<size_t> start_time = 0;
        std::atomic<WorkStatus> status = WorkStatus::STOP;

        int idx = -1;

        std::vector< std::shared_ptr< std::thread > > blocked_threads;

        Worker(std::shared_ptr< std::thread > t) : thread(t), tasks(10), is_running(true), status(WorkStatus::STOP) {}
        Worker() : thread(nullptr), tasks(10), is_running(true), status(WorkStatus::STOP) {}
    };
    using WorkerArray = std::vector< std::shared_ptr< Worker > >;

public:
    ThreadPool(const size_t &size);
    virtual ~ThreadPool();


    virtual void exec();
    virtual void spin_once() = 0;

protected:
    const size_t reserve_;
    volatile size_t size_;
    bool is_working_;


    std::mutex worker_lock_;
    WorkerArray workers_;

private:
    std::shared_ptr<std::thread> task_watcher_worker_;

    void task_watcher_();

    virtual void run_() {}

};

class ThreadBindPool : public ThreadPool
{
public:
    ThreadBindPool(const size_t &size);
    virtual ~ThreadBindPool() override;

    void add_task_to(ThreadPool::TaskPtr task, const size_t &index);
    template<typename F, typename... Args>
    auto add_task_to(const size_t &index, F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;
        static_assert(std::is_same<return_type, RetBool>::value, "return type must be RetBool.");

        TaskPtr task = std::make_shared< Task >();
        task->task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        task->owener_id = pthread_self();
        task->add_time = coin::DateTime::current_date_time().to_usecs_since_epoch();

        {
            std::lock_guard<std::mutex> lock(worker_lock_);
            while(workers_.size() <= (index % reserve_))
            {
                workers_.emplace_back(std::make_shared<ThreadPool::Worker>());
            }
            size_ = workers_.size();

            auto& worker = workers_[index % reserve_];

            worker->tasks.push_back(task);
            worker->idx = index;

            // 检查 Worker 工作时间是否过长
            auto now = coin::DateTime::current_date_time().to_usecs_since_epoch();
            auto start_time = worker->start_time.load();
            if( worker->status.load() == ThreadPool::Worker::WorkStatus::WORKING and
                start_time != 0 and (now - start_time) > 1000000)
            {
                // coin::Print::warn("worker <{}> has worked for a long time, start: {}, interval: {}us.", index, start_time, now - start_time);
                fmt::print("worker <{}> has worked for a long time, start: {}, interval: {}us.\n", index, start_time, now - start_time);
            }

            if(worker->thread != nullptr)
            {
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

        return task->task->get_future();
    }

    virtual void exec() override;
    virtual void spin_once() override;

private:

    void run_(std::shared_ptr< Worker > worker);

};

} // namespace coin