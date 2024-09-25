/**
 * @file local_channal.hpp
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
#include <functional>
#include <stddef.h>
#include <stdint.h>

#include <coin-data/local/impl/shm_shared_ptr.hpp>
#include <coin-data/local/impl/shmobj_manager.hpp>
#include <coin-data/communicator_type.hpp>
#include <type_traits>

namespace coin::data::local
{
template<typename DataT>
class LocalWriter;
template<typename DataT>
class LocalReader;
template<typename DataT>
class LocalPublisher;
template<typename DataT>
class LocalSubscriber;
template<typename ReqT, typename AckT>
class LocalService;
template<typename ReqT, typename AckT>
class LocalClient;

class Communicator
{
    friend class LocalChannal;

public:
    Communicator(const std::string& name);
    ~Communicator();

    inline const std::string name() const noexcept { return name_; }

public:
    template<typename T>
    struct CommunicatorItem
    {
        bool is_online = false;
        uint64_t index = 0;
        T buffer;
        ProcessMutex mutex;

        CommunicatorItem() = default;
        CommunicatorItem(const size_t& bs) : buffer(bs) { }
        ~CommunicatorItem()
        { }
    };

    virtual bool is_online() = 0;

private:
    const std::string name_;

private:
    virtual bool is_ready_() = 0;
    virtual void invoke_() = 0;
    virtual void spin_() = 0;
    virtual bool connecte_to_() = 0;
    virtual void disconnect_() = 0;
};
// class LocalChannal;

class LocalChannal
{

private:
    class LoopTimer
    {
        struct TaskItem
        {
            uint64_t cycle;
            uint64_t last_invoke_time = 0;
            std::function<void()> task;
        };

    public:
        LoopTimer();
        ~LoopTimer();

        void installTask(const uint64_t& cycle, const std::function<void()>& task) noexcept;

        void exec(const uint64_t& time) noexcept;

    private:
        std::list<TaskItem> task_list_;
    };

public:

    LocalChannal(const LocalChannal&) = delete;
    LocalChannal(LocalChannal&&) = delete;
    void operator ()() const = delete;
    LocalChannal& operator = (const LocalChannal&) = delete;
    LocalChannal& operator = (LocalChannal&&) = delete;

    ~LocalChannal();

    void spin_once();

    inline const std::string& selfName() const { return self_name_; }
    static const std::map<std::string, std::string>& communicators();

    static LocalChannal& instance();

    static void init(int argc, char* argv[]);

private:
    LocalChannal();

public:
    template<typename DataT>
    [[nodiscard]] static typename LocalWriter<DataT>::Ptr writer(const std::string& name);

    template<typename DataT>
    [[nodiscard]] static typename LocalReader<DataT>::Ptr reader(const std::string& name);

    template<typename DataT>
    [[nodiscard]] static typename LocalPublisher<DataT>::Ptr publisher(const std::string& name, const std::size_t& bs = 10);

    template<typename DataT>
    [[nodiscard]] static typename LocalSubscriber<DataT>::Ptr subscriber(const std::string& name, const typename LocalSubscriber<DataT>::Callback& cb, const std::size_t& bs = 10);

    template<typename ReqT, typename AckT>
    [[nodiscard]] static typename LocalService<ReqT, AckT>::Ptr service(const std::string& name, const typename LocalService<ReqT, AckT>::Callback& cb, const std::size_t& bs = 10);

    template<typename ReqT, typename AckT>
    [[nodiscard]] static typename LocalClient<ReqT, AckT>::Ptr client(const std::string& name, const std::size_t& bs = 10);

private:

    uint64_t last_update_time_;

    std::string self_name_;

    LoopTimer loop_timer_;

    std::list< std::weak_ptr<Communicator> > wait_connect_list_;
    std::list< std::weak_ptr<Communicator> > connected_list_;
    std::list< std::weak_ptr<Communicator> > work_list_;

    ProcessMutex node_map_mutex_;
    struct NodeMapItem {
        pid_t pid;
    };
    SharedObjectSharedPtr< ShmMap<ShmString, NodeMapItem> > node_map_;

    void check_connect_immediately_();
    void check_connect_status_();

    void init_node_map_();
};


template<typename DataT>
class LocalWriter : public Writer<DataT>, public std::enable_shared_from_this< LocalWriter<DataT> >
{
    friend class LocalChannal;
    friend class Communicator;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalWriter<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:
    LocalWriter(const Private&, const std::string& name)
      : Writer<DataT>(name), buffer_(ShmObjManager::instance().create<Communicator::CommunicatorItem<DataPtr>>(name))
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = true;
    }
    ~LocalWriter()
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = false;
        // 清理插入的元素
        ShmObjManager::shared_obj( buffer_ ).buffer.reset();
    }

    LocalWriter() = delete;
    LocalWriter(const LocalWriter&) = delete;
    void operator ()() const = delete;
    LocalWriter& operator = (const LocalWriter&) = delete;

    static Ptr create(const std::string& name)
    {
        return std::make_shared< LocalWriter<DataT> >(Private(), name);
    }

    virtual void write(const DataPtr& data) override final
    {
        ProcessLockGuard<ProcessMutex> lock(ShmObjManager::shared_obj( buffer_ ).mutex);
        ShmObjManager::shared_obj( buffer_ ).buffer = data;
        ShmObjManager::shared_obj( buffer_ ).index += 1;
    }

private:
    SharedObjectSharedPtr< Communicator::CommunicatorItem<DataPtr> > buffer_;
};

template<typename DataT>
class LocalReader : public Reader<DataT>, public Communicator, public std::enable_shared_from_this< LocalReader<DataT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalReader<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:

    LocalReader(const Private&, const std::string& name)
      : Reader<DataT>(name), Communicator(name)
    {
    }

    LocalReader() = delete;
    LocalReader(const LocalReader&) = delete;
    void operator ()() const = delete;
    LocalReader& operator = (const LocalReader&) = delete;

    static Ptr create(const std::string& name)
    {
        return std::make_shared< LocalReader<DataT> >(Private(), name);
    }

    virtual bool is_online() override final
    {
        return data_buffer_ != nullptr && data_buffer_->get()->is_online;
    }

    virtual bool is_update() override final
    {
        return (data_buffer_) && (data_buffer_->get()) && (idx_ != data_buffer_->get()->index);
    }

    virtual ConstDataPtr read() override final
    {
        ProcessLockGuard<ProcessMutex> lock(data_buffer_->get()->mutex);
        idx_ = data_buffer_->get()->index;
        return data_buffer_->get()->buffer;
    }

private:
    uint64_t idx_;
    SharedObjectSharedPtr< Communicator::CommunicatorItem<DataPtr> > data_buffer_;

private:

    virtual bool is_ready_() override final { return false; }

    virtual void invoke_() override final { }

    virtual void spin_() override final { }

    virtual bool connecte_to_() override final
    {
        data_buffer_ = ShmObjManager::instance().create<Communicator::CommunicatorItem<DataPtr>>(name());
        return data_buffer_ != nullptr;
    }

    virtual void disconnect_() override final
    {
        ShmObjManager::instance().destroy<Communicator::CommunicatorItem<DataPtr>>(name());
        data_buffer_ = nullptr;
    }
};

template<typename DataT>
class LocalPublisher : public Publisher<DataT>, public std::enable_shared_from_this< LocalPublisher<DataT> >
{
    friend class LocalChannal;
    friend class Communicator;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalPublisher<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:
    LocalPublisher(const Private&, const std::string& name, const size_t& bs)
      : Publisher<DataT>(name)
      , buffer_(ShmObjManager::instance().create<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name, bs))
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = true;
    }
    virtual ~LocalPublisher() override final
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = false;
        // 清理插入的元素
        for(auto idx = ShmObjManager::shared_obj( buffer_ ).buffer.head(); idx < ShmObjManager::shared_obj( buffer_ ).buffer.tail(); idx++)
        {
            ShmObjManager::shared_obj( buffer_ ).buffer[idx].reset();
        }
    }

    LocalPublisher() = delete;
    LocalPublisher(const LocalPublisher&) = delete;
    void operator ()() const = delete;
    LocalPublisher& operator = (const LocalPublisher&) = delete;

    static Ptr create(const std::string& name, const size_t& bs)
    {
        return std::make_shared< LocalPublisher<DataT> >(Private(), name, bs);
    }

    virtual void publish(const DataPtr& data) override final
    {
        ShmObjManager::shared_obj( buffer_ ).buffer.push_back(data);
        ShmObjManager::shared_obj( buffer_ ).index += 1;
    }

private:

    SharedObjectSharedPtr< Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > buffer_;
};

template<typename DataT>
class LocalSubscriber : public Subscriber<DataT>, public Communicator, public std::enable_shared_from_this< LocalSubscriber<DataT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalSubscriber<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

    using Callback = std::function< void(ConstDataPtr) >;
public:

    LocalSubscriber(const Private&, const std::string& name, const Callback& cb, const size_t& bs)
      : Subscriber<DataT>(name)
      , Communicator(name)
      , cb_(cb)
      , buffer_size_(bs)
      , idx_(0)
    { }

    ~LocalSubscriber() = default;

    LocalSubscriber() = delete;
    LocalSubscriber(const LocalSubscriber&) = delete;
    void operator ()() const = delete;
    LocalSubscriber& operator = (const LocalSubscriber&) = delete;

    static Ptr create(const std::string& name, const Callback& cb, const size_t& bs)
    {
        return std::make_shared< LocalSubscriber<DataT> >(Private(), name, cb, bs);
    }

    virtual bool is_online() override final
    {
        return data_buffer_ != nullptr && data_buffer_->get()->is_online;
    }

    virtual bool is_update() override final
    {
        return (data_buffer_) && (data_buffer_->get()) && (idx_ != data_buffer_->get()->index);
    }

private:
    const Callback cb_;
    const size_t buffer_size_;
    size_t idx_;
    std::mutex buffer_mutex_;
    ShmDeque<DataPtr> buffer_;

    SharedObjectSharedPtr< Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > data_buffer_;

private:

    virtual bool is_ready_() override final
    {
        if(idx_ < data_buffer_->get()->buffer.head())
        {
            idx_ = data_buffer_->get()->buffer.head();
        }
        return idx_ < data_buffer_->get()->buffer.tail();
    }

    virtual void invoke_() override final
    {
        auto it = data_buffer_->get()->buffer.copy(idx_);
        if(not it.get())
        {
            coin::Print::error("null itor");
            abort();
        }
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_.push_back(it);
            if(buffer_.size() > buffer_size_)
            {
                buffer_.pop_front();
            }
        }
        idx_ += 1;
    }

    virtual void spin_() override final
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if(not buffer_.empty())
        {
            cb_(buffer_.front());
            buffer_.pop_front();
        }
    }

    virtual bool connecte_to_() override final
    {
        data_buffer_ = ShmObjManager::instance().create<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name(), buffer_size_);
        idx_ = data_buffer_->get()->buffer.tail();
        return data_buffer_ != nullptr;
    }

    virtual void disconnect_() override final
    {
        ShmObjManager::instance().destroy<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name());
        data_buffer_ = nullptr;
    }
};

template<typename ReqT>
struct RequestData__
{
    pid_t pid;
    const ShmSharedPtr<ReqT> ptr;

    RequestData__(const ShmSharedPtr<ReqT>& p) : pid(0), ptr(p) {}
    ~RequestData__() { }
};

template<typename AckT>
struct AckData__
{
    pid_t pid;
    bool is_ready;
    ShmSharedPtr<AckT> ptr;

    AckData__(const ShmSharedPtr<AckT>& p) : pid(0), is_ready(false), ptr(p) {}
    ~AckData__() { }
};

template<typename ReqT, typename AckT>
class LocalService : public Service<ReqT, AckT>, public Communicator, public std::enable_shared_from_this< Service<ReqT, AckT> >
{
    friend class LocalChannal;
    friend class Communicator;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalService<ReqT, AckT>>;

    using ReqType = ReqT;
    using AckType = AckT;
    using ReqPtr = ShmSharedPtr<ReqType>;
    using AckPtr = ShmSharedPtr<AckType>;
    using ConstReqPtr = const ReqPtr;
    using ConstAckPtr = const AckPtr;
    using Callback = std::function< bool(ConstReqPtr&, AckPtr&) >;

    using DataType = std::pair<RequestData__<ReqType>, AckData__<AckType>>;
    using DataPtr = ShmSharedPtr<DataType>;

public:
    LocalService(const Private&, const std::string& name, const Callback& cb, const size_t& bs)
      : Service<ReqT, AckT>(name)
      , Communicator(name)
      , cb_(cb)
      , buffer_(ShmObjManager::instance().create<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name, bs))
      , idx_(ShmObjManager::shared_obj( buffer_ ).buffer.tail())
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = true;
    }
    ~LocalService()
    {
        ShmObjManager::shared_obj( buffer_ ).is_online = false;
        // 清理插入的元素
        for(auto idx = ShmObjManager::shared_obj( buffer_ ).buffer.head(); idx < ShmObjManager::shared_obj( buffer_ ).buffer.tail(); idx++)
        {
            ShmObjManager::shared_obj( buffer_ ).buffer[idx].reset();
        }
    }

    LocalService() = delete;
    LocalService(const LocalService&) = delete;
    void operator ()() const = delete;
    LocalService& operator = (const LocalService&) = delete;

    static Ptr create(const std::string& name, const Callback& cb, const size_t& bs)
    {
        return std::make_shared< LocalService<ReqT, AckT> >(Private(), name, cb, bs);
    }

    virtual bool is_online() override final
    {
        return buffer_ != nullptr && ShmObjManager::shared_obj( buffer_ ).is_online;
    }

    virtual bool is_update() override final
    {
        return (buffer_) && (buffer_.get()) && (idx_ != ShmObjManager::shared_obj( buffer_ ).index);
    }

private:
    Callback cb_;
    std::mutex buffer_mutex_;
    SharedObjectSharedPtr< Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > buffer_;
    uint64_t idx_;

private:

    virtual bool is_ready_() override final
    {
        if(idx_ < ShmObjManager::shared_obj( buffer_ ).buffer.head())
        {
            idx_ = ShmObjManager::shared_obj( buffer_ ).buffer.head();
        }
        return idx_ < ShmObjManager::shared_obj( buffer_ ).buffer.tail();
    }

    virtual void invoke_() override final
    {

    }

    virtual void spin_() override final
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if(is_ready_())
        {
            auto& it = ShmObjManager::shared_obj( buffer_ ).buffer[idx_];
            cb_(it->first.ptr, it->second.ptr);
            it->second.is_ready = true;
            idx_ += 1;
        }
    }

    virtual bool connecte_to_() override final
    {
        return true;
    }

    virtual void disconnect_() override final
    {
        ShmObjManager::instance().destroy<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name());
        buffer_ = nullptr;
    }
};

template<typename ReqT, typename AckT>
class LocalClient : public Client<ReqT, AckT>, public Communicator, public std::enable_shared_from_this< Client<ReqT, AckT> >
{
    friend class LocalChannal;
    struct Private {};
public:

    using Ptr = std::shared_ptr<LocalClient<ReqT, AckT>>;

    using ReqType = ReqT;
    using AckType = AckT;
    using ReqPtr = ShmSharedPtr<ReqType>;
    using AckPtr = ShmSharedPtr<AckType>;
    using ConstReqPtr = const ReqPtr;
    using ConstAckPtr = const AckPtr;
    using Callback = std::function< bool(ConstReqPtr&, AckPtr&) >;

    using DataType = std::pair<RequestData__<ReqType>, AckData__<AckType>>;
    using DataPtr = ShmSharedPtr<DataType>;
public:

    LocalClient(const Private&, const std::string& name, const size_t& bs)
      : Client<ReqT, AckT>(name)
      , Communicator(name)
      , buffer_size_(bs)
    { }

    LocalClient() = delete;
    LocalClient(const LocalClient&) = delete;
    void operator ()() const = delete;
    LocalClient& operator = (const LocalClient&) = delete;

    static Ptr create(const std::string& name, const size_t& bs)
    {
        return std::make_shared< LocalClient<ReqT, AckT> >(Private(), name, bs);
    }

    virtual bool is_online() override final
    {
        return data_buffer_ != nullptr && data_buffer_->get()->is_online;
    }

    virtual bool is_update() override final
    {
        return false;
    }

    virtual bool call(ConstReqPtr& req, AckPtr& ack) override final
    {
        if(not data_buffer_)
        {
            return false;
        }

        DataPtr ptr = makeShmShared<DataType>(req, ack);
        ptr->first.pid = getpid();
        ptr->second.is_ready = false;
        data_buffer_->get()->buffer.push_back( ptr );

        while(not ptr->second.is_ready && coin::ok())
        {
            usleep(1);
            if(not data_buffer_->get()->is_online)
            {
                return false;
            }
        }
        auto ret = ptr->second.is_ready;

        return ret;
    }

private:
    const Callback cb_;
    const size_t buffer_size_;
    std::mutex buffer_mutex_;
    ShmDeque<DataPtr> buffer_;

    SharedObjectSharedPtr< Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > data_buffer_;

private:

    virtual bool is_ready_() override final { return false; }

    virtual void invoke_() override final { }

    virtual void spin_() override final { }

    virtual bool connecte_to_() override final
    {
        data_buffer_ = ShmObjManager::instance().create<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name(), buffer_size_);
        return data_buffer_ != nullptr;
    }

    virtual void disconnect_() override final
    {
        ShmObjManager::instance().destroy<Communicator::CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name());
        data_buffer_ = nullptr;
    }

};

template <typename DataT>
inline typename LocalWriter<DataT>::Ptr LocalChannal::writer(const std::string &name)
{
    auto w = LocalWriter<DataT>::create(name);
    LocalChannal::instance().check_connect_immediately_();
    return w;
}

template <typename DataT>
inline typename LocalReader<DataT>::Ptr LocalChannal::reader(const std::string &name)
{
    auto reader = LocalReader<DataT>::create(name);
    if(reader)
    {
        instance().wait_connect_list_.push_back( std::static_pointer_cast<Communicator>(reader) );
    }
    LocalChannal::instance().check_connect_immediately_();
    return reader;
}

template <typename DataT>
inline typename LocalPublisher<DataT>::Ptr LocalChannal::publisher(const std::string &name, const std::size_t &bs)
{
    auto pub = LocalPublisher<DataT>::create(name, bs);
    LocalChannal::instance().check_connect_immediately_();
    return pub;
}

template <typename DataT>
inline typename LocalSubscriber<DataT>::Ptr LocalChannal::subscriber(const std::string &name, const typename LocalSubscriber<DataT>::Callback &cb, const std::size_t &bs)
{
    auto sub = LocalSubscriber<DataT>::create(name, cb, bs);
    if(sub)
    {
        LocalChannal::instance().wait_connect_list_.push_back(sub);
    }
    LocalChannal::instance().check_connect_immediately_();
    return sub;
}

template <typename ReqT, typename AckT>
inline typename LocalService<ReqT, AckT>::Ptr LocalChannal::service(const std::string &name, const typename LocalService<ReqT, AckT>::Callback &cb, const std::size_t &bs)
{
    auto ser = LocalService<ReqT, AckT>::create(name, cb, bs);
    if(ser)
    {
        LocalChannal::instance().work_list_.push_back(ser);
    }
    else
    {
        coin::Print::warn("local channal service <{}> create failed.", name);
    }

    LocalChannal::instance().check_connect_immediately_();
    return ser;
}

template <typename ReqT, typename AckT>
inline typename LocalClient<ReqT, AckT>::Ptr LocalChannal::client(const std::string &name, const std::size_t &bs)
{
    auto client = LocalClient<ReqT, AckT>::create(name, bs);
    if(client)
    {
        LocalChannal::instance().wait_connect_list_.push_back(client);
    }
    else
    {
        coin::Print::info("local channal client <{}> create failed.", name);
    }

    LocalChannal::instance().check_connect_immediately_();
    return client;
}

} // namespace coin::data
