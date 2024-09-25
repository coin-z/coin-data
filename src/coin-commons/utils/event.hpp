/**
 * @file event.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <memory>
#include <deque>
#include <mutex>

#include <coin-commons/utils/threadpool.hpp>


namespace coin
{

class EventPool
{
    class EventBase
    {
    public:
        EventBase() = default;
        virtual ~EventBase() = default;
        virtual operator bool() = 0;
    };
    using EventBasePtr = std::shared_ptr< EventBase >;

    class TaskBase
    {
    public:
        TaskBase() = default;
        virtual ~TaskBase() = default;

        virtual RetBool invoke() = 0;
    };
    using TaskBasePtr = std::shared_ptr< TaskBase >;

private:
    template<typename T>
    class Event : public EventBase
    {
        static_assert(std::is_invocable_r_v<bool, T>, "Event must be a callable object and return a bool.");
    public:
        Event(const T& t);
        ~Event();
        operator bool() override final;
    private:
        T event_;
    };

    template<typename T>
    class Task : public TaskBase
    {
        static_assert(std::is_invocable_r_v<void, T>, "Task must be a callable object and return a bool.");
    public:
        Task(const T& t);
        ~Task();
        RetBool invoke() override final;
    private:
        T task_;
    };

    template<typename T>
    using EventPtr = std::shared_ptr< Event<T> >;
    template<typename T>
    using TaskPtr = std::shared_ptr< Task<T> >;

public:
    EventPool();
    ~EventPool();

    template<typename EventT, typename TaskT>
    void push_event(const EventT& event, const TaskT& task);

    void run_and_clear_all_ready_task();
    void clear_all_ready_task();

    void spin_once();

private:
    std::mutex event_deque_mutex_;
    std::deque<std::pair<EventBasePtr, TaskBasePtr>> event_deque_;
    std::mutex ready_task_deque_mutex_;
    std::deque<TaskBasePtr> ready_task_deque_;
    std::shared_ptr<coin::ThreadBindPool> thread_pool_;
};

template <typename T>
inline EventPool::Event<T>::Event(const T& t) : event_(t)
{ }

template <typename T>
inline EventPool::Event<T>::~Event()
{ }

template <typename T>
inline EventPool::Event<T>::operator bool()
{ return event_(); }

template<typename EventT, typename TaskT>
inline void EventPool::push_event(const EventT& event, const TaskT& task)
{
    std::lock_guard<std::mutex> lock(event_deque_mutex_);
    event_deque_.push_back({ std::make_shared<Event<EventT>>(event), std::make_shared<Task<TaskT>>(task)});
}

template <typename T>
inline EventPool::Task<T>::Task(const T& t) : task_(t)
{ }

template <typename T>
inline EventPool::Task<T>::~Task()
{ }

template <typename T>
inline RetBool EventPool::Task<T>::invoke()
{ task_(); return false; }

} // namespace coin
