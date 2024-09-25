/**
 * @file communicator.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once
#include <stddef.h>

#include <coin-data/communicator_type.hpp>
#include <coin-data/local/local_channal.hpp>


namespace coin::data
{ 

class MutableCommunicator
{
public:
    using Ptr = std::shared_ptr<MutableCommunicator>;

    MutableCommunicator(const std::string& id);
    virtual ~MutableCommunicator() = default;

    inline std::string id() { return id_; }
    virtual std::string target_name() = 0;

    virtual void redirect(const std::string& name) = 0;
    virtual void disconnect() = 0;

private:
    const std::string id_;
};

template<typename DataT> class MutableWriter;
template<typename DataT> class MutableReader;

class Communicator
{
template<typename DataT> friend class MutableWriter;
template<typename DataT> friend class MutableReader;
template<typename DataT> friend class MutablePublisher;
template<typename DataT> friend class MutableSubscriber;
public:
    Communicator();
    Communicator(const Communicator&) = delete;
    Communicator& operator=(const Communicator&) = delete;
    ~Communicator();

    static void init(int argc, char *argv[]);

    static void spin_once();

    static const std::string& node_name();

    template<typename DataT>
    [[nodiscard]] static typename Writer<DataT>::Ptr writer(const std::string& name);

    template<typename DataT>
    [[nodiscard]] static typename Reader<DataT>::Ptr reader(const std::string& name);

    template<typename DataT>
    [[nodiscard]] static typename Publisher<DataT>::Ptr publisher(const std::string& name, const std::size_t& bs = 10);

    template<typename DataT>
    [[nodiscard]] static typename Subscriber<DataT>::Ptr subscriber(const std::string& name, const typename Subscriber<DataT>::Callback& cb, const std::size_t &bs = 10);

    template<typename ReqT, typename AckT>
    [[nodiscard]] static typename Service<ReqT, AckT>::Ptr service(const std::string& name, const typename Service<ReqT, AckT>::Callback& cb, const std::size_t &bs = 10);

    template<typename ReqT, typename AckT>
    [[nodiscard]] static typename Client<ReqT, AckT>::Ptr client(const std::string& name, const std::size_t& bs = 10);

    struct MutableServerReq
    {
        coin::data::ShmString comm_id;
        coin::data::ShmString target_name;
    };
    struct MutableServerAck
    {
        bool is_ok;
        coin::data::ShmString msg;
    };


private:
    static std::unique_ptr< std::map<std::string, MutableCommunicator::Ptr> >& mutable_communicator();

    static Service<MutableServerReq, MutableServerAck>::Ptr mutable_communicator_service_;

    static void register_mutable_communicator_(const std::string& id, const MutableCommunicator::Ptr& mc);
    static bool mutable_communicator_service_callback_(
        Service<MutableServerReq, MutableServerAck>::ConstReqPtr& req,
        Service<MutableServerReq, MutableServerAck>::AckPtr& ack
    );
};

template <typename DataT>
inline typename Writer<DataT>::Ptr Communicator::writer(const std::string &name)
{
    return local::LocalChannal::writer<DataT>(name);
}

template <typename DataT>
inline typename Reader<DataT>::Ptr Communicator::reader(const std::string &name)
{
    return local::LocalChannal::reader<DataT>(name);
}

template <typename DataT>
inline typename Publisher<DataT>::Ptr Communicator::publisher(const std::string &name, const std::size_t &bs)
{
    return local::LocalChannal::publisher<DataT>(name, bs);
}

template <typename DataT>
inline typename Subscriber<DataT>::Ptr Communicator::subscriber(const std::string &name, const typename Subscriber<DataT>::Callback& cb, const std::size_t &bs)
{
    return local::LocalChannal::subscriber<DataT>(name, cb, bs);
}

template <typename ReqT, typename AckT>
inline typename Service<ReqT, AckT>::Ptr Communicator::service(const std::string &name, const typename Service<ReqT, AckT>::Callback &cb, const std::size_t &bs)
{
    return local::LocalChannal::service<ReqT, AckT>(name, cb, bs);
}

template <typename ReqT, typename AckT>
inline typename Client<ReqT, AckT>::Ptr Communicator::client(const std::string &name, const std::size_t &bs)
{
    return local::LocalChannal::client<ReqT, AckT>(name, bs);
}

template<typename DataT>
class MutableWriter : public MutableCommunicator
{

public:
    using Ptr = std::shared_ptr<MutableWriter<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

    explicit MutableWriter(const std::string& id) : MutableCommunicator(id){}
    ~MutableWriter() = default;

    virtual std::string target_name() override final
    {
        if(writer_)
        {
            return writer_->name();
        }
        return "";
    }

    void write(const DataPtr& data)
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        if(writer_)
        {
            writer_->write(data);
        }
    }

    virtual void redirect(const std::string& name) override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        writer_ = Communicator::writer<DataT>(name);
    }

    virtual void disconnect() override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        writer_.reset();
    }

    static Ptr create(const std::string& id);

private:
    std::mutex redirect_mutex_;
    typename Writer<DataT>::Ptr writer_;

};

template<typename DataT>
class MutableReader : public MutableCommunicator
{
public:
    using Ptr = std::shared_ptr<MutableReader<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

    explicit MutableReader(const std::string& id) : MutableCommunicator(id) { }
    ~MutableReader() = default;
    virtual std::string target_name() override final
    {
        if(reader_)
        {
            return reader_->name();
        }
        return "";
    }
    ConstDataPtr read()
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        if(reader_)
        {
            return reader_->read();
        }
        return nullptr;
    }

    virtual void redirect(const std::string& name) override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        reader_ = Communicator::reader<DataT>(name);
    }

    virtual void disconnect() override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        reader_.reset();
    }

    static Ptr create(const std::string& id);

private:
    std::mutex redirect_mutex_;
    typename Reader<DataT>::Ptr reader_;

};

template<typename DataT>
class MutableSubscriber : public MutableCommunicator
{
public:
    using Ptr = std::shared_ptr<MutableSubscriber<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

    explicit MutableSubscriber(
        const std::string& id,
        const typename Subscriber<DataT>::Callback& cb = nullptr)
     : MutableCommunicator(id)
     , sub_callback_(cb) { }
    ~MutableSubscriber() = default;
    virtual std::string target_name() override final
    {
        if(subscriber_)
        {
            return subscriber_->name();
        }
        return "";
    }
    void set_sub_callback(const typename Subscriber<DataT>::Callback& cb)
    {
        sub_callback_ = cb;
        if(not sub_name_.empty())
        {
            coin::Print::info("set sub callback to {}", sub_name_);
            subscriber_ = Communicator::subscriber<DataT>(sub_name_, sub_callback_);
        }
    }

    virtual void redirect(const std::string& name) override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        sub_name_ = name;
        if(sub_callback_)
        {
            subscriber_.reset();
            coin::Print::info("redirect to {}", name);
            subscriber_ = Communicator::subscriber<DataT>(name, sub_callback_);
        }
    }

    virtual void disconnect() override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        subscriber_.reset();
    }

    static Ptr create(const std::string& id, const typename Subscriber<DataT>::Callback& cb = nullptr);

private:
    std::mutex redirect_mutex_;
    typename Subscriber<DataT>::Ptr subscriber_;
    std::string sub_name_;
    typename Subscriber<DataT>::Callback sub_callback_;
};

template<typename DataT>
class MutablePublisher : public MutableCommunicator
{
public:
    using Ptr = std::shared_ptr<MutablePublisher<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

    explicit MutablePublisher(const std::string& id) : MutableCommunicator(id){ }
    ~MutablePublisher() = default;
    virtual std::string target_name() override final
    {
        if(publisher_)
        {
            return publisher_->name();
        }
        return "";
    }
    void publish(const DataPtr& data)
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        if(publisher_) publisher_->publish(data);
    }

    virtual void redirect(const std::string& name) override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        publisher_.reset();
        publisher_ = Communicator::publisher<DataT>(name);
    }

    virtual void disconnect() override final
    {
        std::lock_guard<std::mutex> lock(redirect_mutex_);
        publisher_.reset();
    }

    static Ptr create(const std::string& id);

private:
    std::mutex redirect_mutex_;
    typename Publisher<DataT>::Ptr publisher_;
};


template <typename DataT>
inline typename MutableWriter<DataT>::Ptr MutableWriter<DataT>::create(const std::string &id)
{
    auto obj = std::make_shared<MutableWriter<DataT>>(id);
    Communicator::register_mutable_communicator_(id, obj);
    return obj;
}

template <typename DataT>
inline typename MutableReader<DataT>::Ptr MutableReader<DataT>::create(const std::string &id)
{
    auto obj = std::make_shared<MutableReader<DataT>>(id);
    Communicator::register_mutable_communicator_(id, obj);
    return obj;
}

template <typename DataT>
inline typename MutableSubscriber<DataT>::Ptr MutableSubscriber<DataT>::create(const std::string &id, const typename Subscriber<DataT>::Callback& cb)
{
    auto obj = std::make_shared<MutableSubscriber<DataT>>(id, cb);
    Communicator::register_mutable_communicator_(id, obj);
    return obj;
}

template<typename DataT>
inline typename MutablePublisher<DataT>::Ptr MutablePublisher<DataT>::create(const std::string& id)
{
    auto obj = std::make_shared<MutablePublisher<DataT>>(id);
    Communicator::register_mutable_communicator_(id, obj);
    return obj;
}

} // namespace coin::data
