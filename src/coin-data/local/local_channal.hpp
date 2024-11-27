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
#include <stddef.h>
#include <stdint.h>
#include <functional>
#include <type_traits>

#include <coin-data/communicator_type.hpp>
#include <coin-data/local/impl/shmobj_manager.hpp>

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

template<typename T>
struct CommunicatorItem
{
    bool is_online = false;
    uint64_t index = 0;
    T buffer;
    ProcessMutex mutex;

    CommunicatorItem() = default;
    CommunicatorItem(const size_t& bs) : buffer(bs) { }
    ~CommunicatorItem() = default;
};

// class LocalChannal;
class LocalChannal
{
public:

    LocalChannal(const LocalChannal&) = delete;
    LocalChannal(LocalChannal&&) = delete;
    void operator ()() const = delete;
    LocalChannal& operator = (const LocalChannal&) = delete;
    LocalChannal& operator = (LocalChannal&&) = delete;

    ~LocalChannal();

    inline const std::string& self_name() const { return self_name_; }
    static const std::map<std::string, std::string>& communicators();

    static LocalChannal& instance();

    static void init(int argc, char* argv[], const std::string& name = "");

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
    std::string self_name_;
};
template<typename DataT>
class LocalWriter : public Writer<DataT>, public std::enable_shared_from_this< LocalWriter<DataT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalWriter<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:
    LocalWriter(const Private&, const std::string& name)
      : Writer<DataT>(name)
      , data_(ShmObjManager::instance().create<DataT>(name))
    { }
    ~LocalWriter()
    {
        ShmObjManager::instance().destroy<DataT>(Writer<DataT>::name_);
    }

    LocalWriter() = delete;
    LocalWriter(const LocalWriter&) = delete;
    void operator ()() const = delete;
    LocalWriter& operator = (const LocalWriter&) = delete;

    static Ptr create(const std::string& name)
    {
        return std::make_shared< LocalWriter<DataT> >(Private(), name);
    }

    virtual void write(const DataT& data) override final
    {
        ShmObjManager::lock_object( data_, [this, &data](){
            ShmObjManager::shared_obj( data_ ) = data;
        } );
    }

private:
    SharedObjectSharedPtr<DataT> data_;
};

template<typename DataT>
class LocalReader : public Reader<DataT>, public std::enable_shared_from_this< LocalReader<DataT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalReader<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:

    LocalReader(const Private&, const std::string& name)
      : Reader<DataT>(name)
      , data_(ShmObjManager::instance().discovery<DataT>(Reader<DataT>::name_))
    {
    }

    virtual ~LocalReader()
    {
        ShmObjManager::instance().release<DataT>(Reader<DataT>::name_);
    }

    LocalReader() = delete;
    LocalReader(const LocalReader&) = delete;
    void operator ()() const = delete;
    LocalReader& operator = (const LocalReader&) = delete;

    static Ptr create(const std::string& name)
    {
        return std::make_shared< LocalReader<DataT> >(Private(), name);
    }

    virtual DataT& read() override final
    {
        return ShmObjManager::shared_obj( data_ );
    }

    bool lock(const std::function<void()>& area)
    {
        return ShmObjManager::lock_object( data_, [this, &area]{
            // if(ShmObjManager::shared_obj( data_ ))
            {
                area();
            }
        } );
        return false;
    }

private:
    SharedObjectSharedPtr< DataT > data_;
};

template<typename DataT>
class LocalPublisher : public Publisher<DataT>, public std::enable_shared_from_this< LocalPublisher<DataT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalPublisher<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const ShmSharedPtr<DataT>;

public:
    LocalPublisher(const Private&, const std::string& name, const size_t& bs)
      : coin::data::Publisher<DataT>(name)
      , buffer_(ShmObjManager::instance().create<CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name, bs))
    {
        ShmObjManager::instance().this_node->add_event(Publisher<DataT>::name_);
    }
    virtual ~LocalPublisher() override final
    {
        // clear buffer
        ShmObjManager::lock_object(buffer_, [this](){
            for(auto idx = ShmObjManager::shared_obj( buffer_ ).buffer.head(); idx < ShmObjManager::shared_obj( buffer_ ).buffer.tail(); idx++)
            {
                ShmObjManager::shared_obj( buffer_ ).buffer[idx].reset();
            }
        });
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
        ShmObjManager::lock_object(buffer_, [this, &data](){
            ShmObjManager::shared_obj( buffer_ ).buffer.push_back(data);
            ShmObjManager::shared_obj( buffer_ ).index += 1;
            ShmObjManager::instance().this_node->notify(Publisher<DataT>::name_);
        });
    }

private:
    SharedObjectSharedPtr< CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > buffer_;
};

template<typename DataT>
class LocalSubscriber : public Subscriber<DataT>, public std::enable_shared_from_this< LocalSubscriber<DataT> >
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
      , cb_(cb)
      , buffer_size_(bs)
      , idx_(0)
    {
        auto idx = ShmObjManager::read_node_idx(name);
        data_buffer_ = ShmObjManager::instance().discovery<CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name);
        coin::Print::debug("monitor node: {} - {}", idx.first, idx.second);
        auto ret = node::NodeMonitor::monitor().add_event_to(idx.first, idx.second,
        [this](const std::string& node, const std::string& event, std::any& obj)
        {
            ShmObjManager::lock_object(data_buffer_, [this, &node, &event](){
                if(idx_ < ShmObjManager::shared_obj( data_buffer_ ).index - ShmObjManager::shared_obj( data_buffer_ ).buffer.size())
                {
                    idx_ = ShmObjManager::shared_obj( data_buffer_ ).index - 1;
                }
                while(idx_ < ShmObjManager::shared_obj( data_buffer_ ).index)
                {
                    cb_(ShmObjManager::shared_obj( data_buffer_ ).buffer.copy(idx_));
                    idx_ += 1;
                }
            });

        });
        if(ret)
        {
            coin::Print::info("add event to {}", name);
        }
        else
        {
            coin::Print::error("add event to {}", name);
        }
    }

    ~LocalSubscriber() = default;

    LocalSubscriber() = delete;
    LocalSubscriber(const LocalSubscriber&) = delete;
    void operator ()() const = delete;
    LocalSubscriber& operator = (const LocalSubscriber&) = delete;

    static Ptr create(const std::string& name, const Callback& cb, const size_t& bs)
    {
        return std::make_shared< LocalSubscriber<DataT> >(Private(), name, cb, bs);
    }

private:
    const Callback cb_;
    const size_t buffer_size_;
    size_t idx_;

    SharedObjectSharedPtr< CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > data_buffer_;
};

template<typename ReqT>
struct RequestData__
{
    pid_t pid;
    ShmSharedPtr<ReqT> ptr;

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
class LocalService : public Service<ReqT, AckT>, public std::enable_shared_from_this< Service<ReqT, AckT> >
{
    friend class LocalChannal;
    struct Private {};
public:
    using Ptr = std::shared_ptr<LocalService<ReqT, AckT>>;

    using ReqType = ReqT;
    using AckType = AckT;
    using ReqPtr = ShmSharedPtr<ReqType>;
    using AckPtr = ShmSharedPtr<AckType>;
    using ConstReqPtr = const ReqPtr;
    using ConstAckPtr = const AckPtr;
    using Callback = std::function< bool(const ReqType&, AckType&) >;

    using DataType = std::pair<RequestData__<ReqType>, AckData__<AckType>>;
    using DataPtr = ShmSharedPtr<DataType>;

public:
    LocalService(const Private&, const std::string& name, const Callback& cb, const size_t& bs)
      : Service<ReqT, AckT>(name)
      , cb_(cb)
      , buffer_(ShmObjManager::instance().create<CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name, bs))
      , idx_(ShmObjManager::shared_obj( buffer_ ).buffer.tail())
    {
        for(size_t i = 0; i < bs; i++)
        {
            ShmObjManager::shared_obj( buffer_ ).buffer.push_back(
                make_shared_obj<DataType>(std::make_pair<RequestData__<ReqType>, AckData__<AckType>>(
                    make_shared_obj<ReqType>(), make_shared_obj<AckType>()
                ))
            );
        }

        ShmObjManager::shared_obj( buffer_ ).is_online = true;
        coin::Print::debug("monitor node: {} - {}", ShmObjManager::instance().node_name(), name);
        ShmObjManager::instance().this_node->add_event(name);
        node::NodeMonitor::monitor().add(ShmObjManager::instance().node_name());
        auto ret = node::NodeMonitor::monitor().add_event_to(ShmObjManager::instance().node_name(), name,
        [this](const std::string& node, const std::string& event, std::any& obj)
        {
            coin::Print::info("read req from client");
            ShmObjManager::lock_object(buffer_, [this, &node, &event](){
                if(idx_ < ShmObjManager::shared_obj( buffer_ ).index - ShmObjManager::shared_obj( buffer_ ).buffer.size())
                {
                    idx_ = ShmObjManager::shared_obj( buffer_ ).index - 1;
                }
                while(idx_ < ShmObjManager::shared_obj( buffer_ ).index)
                {
                    auto& it = ShmObjManager::shared_obj( buffer_ ).buffer[idx_];
                    cb_(*it->first.ptr, *it->second.ptr);
                    it->second.is_ready = true;
                    idx_ += 1;
                }
            });
        });
        if(ret)
        {
            coin::Print::info("add event to {}, {}", ShmObjManager::instance().node_name(), name);
        }
        else
        {
            coin::Print::error("add event to {}, {}", ShmObjManager::instance().node_name(), name);
        }
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

private:
    Callback cb_;
    SharedObjectSharedPtr< CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > buffer_;
    uint64_t idx_;
};

template<typename ReqT, typename AckT>
class LocalClient : public Client<ReqT, AckT>, public std::enable_shared_from_this< Client<ReqT, AckT> >
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
      , buffer_size_(bs)
      , data_buffer_(ShmObjManager::instance().discovery<CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>>>(name))
    {
    }

    LocalClient() = delete;
    LocalClient(const LocalClient&) = delete;
    void operator ()() const = delete;
    LocalClient& operator = (const LocalClient&) = delete;

    static Ptr create(const std::string& name, const size_t& bs)
    {
        return std::make_shared< LocalClient<ReqT, AckT> >(Private(), name, bs);
    }
    virtual bool call(std::function<void(ReqType&)>&& req, std::function<void(const AckType&)>&& ack) override final
    {
        bool ret = false;
        size_t obj_idx = 0;
        bool lock_ret = ShmObjManager::lock_object(data_buffer_, [this, &req, &obj_idx](){
            obj_idx = ShmObjManager::shared_obj( data_buffer_ ).index;
            auto& it = ShmObjManager::shared_obj( data_buffer_ ).buffer[obj_idx];
            req(*(it->first.ptr));
            ShmObjManager::shared_obj( data_buffer_ ).index += 1;
        });

        auto node_idx = ShmObjManager::read_node_idx(Client<ReqT, AckT>::name_);
        ret = node::NodeMonitor::notify_event(node_idx.first, node_idx.second);

        // wait for ready
        bool is_ready = false;
        while(not is_ready && coin::ok())
        {
            bool lock_ret = ShmObjManager::lock_object(data_buffer_, [this, &ret, &is_ready, &ack, &obj_idx](){
                auto& it = ShmObjManager::shared_obj( data_buffer_ ).buffer[obj_idx];
                
                if(it->second.is_ready)
                {
                    ack(*(it->second.ptr));
                    is_ready = true;
                    ret = true;
                }
            });
            if(not lock_ret) break;
            usleep(10);
        }

        return ret && lock_ret;
    }
private:
    const Callback cb_;
    const size_t buffer_size_;
    std::mutex buffer_mutex_;

    SharedObjectSharedPtr< CommunicatorItem<coin::data::__inner::SharedCircularBuffer<DataPtr>> > data_buffer_;
};
template <typename DataT>
inline typename LocalWriter<DataT>::Ptr LocalChannal::writer(const std::string &name)
{
    auto w = LocalWriter<DataT>::create(name);
    return w;
}
template <typename DataT>
inline typename LocalReader<DataT>::Ptr LocalChannal::reader(const std::string &name)
{
    auto reader = LocalReader<DataT>::create(name);
    return reader;
}
template <typename DataT>
inline typename LocalPublisher<DataT>::Ptr LocalChannal::publisher(const std::string &name, const std::size_t &bs)
{
    auto pub = LocalPublisher<DataT>::create(name, bs);
    return pub;
}
template <typename DataT>
inline typename LocalSubscriber<DataT>::Ptr LocalChannal::subscriber(const std::string &name, const typename LocalSubscriber<DataT>::Callback &cb, const std::size_t &bs)
{
    auto sub = LocalSubscriber<DataT>::create(name, cb, bs);
    return sub;
}
template <typename ReqT, typename AckT>
inline typename LocalService<ReqT, AckT>::Ptr LocalChannal::service(const std::string &name, const typename LocalService<ReqT, AckT>::Callback &cb, const std::size_t &bs)
{
    auto ser = LocalService<ReqT, AckT>::create(name, cb, bs);
    return ser;
}
template <typename ReqT, typename AckT>
inline typename LocalClient<ReqT, AckT>::Ptr LocalChannal::client(const std::string &name, const std::size_t &bs)
{
    auto client = LocalClient<ReqT, AckT>::create(name, bs);
    return client;
}
} // namespace coin::data
