/**
 * @file communicator_type.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <string>
#include <memory>
#include <coin-data/local/data.hpp>

namespace coin::data
{

template<typename DataT>
class Writer
{
public:
    using Ptr = std::shared_ptr<Writer<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

public:
    explicit Writer(const std::string& name) : name_(name) {}
    virtual ~Writer() = default;

    virtual void write(const DataPtr& data) = 0;

    inline std::string name() const { return name_; }
protected:
    const std::string name_;
};

template<typename DataT>
class Reader
{
public:
    using Ptr = std::shared_ptr<Reader<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

public:
    explicit Reader(const std::string& name) : name_(name) {}
    virtual ~Reader() = default;

    virtual bool is_update() = 0;
    virtual ConstDataPtr read() = 0;

    inline std::string name() const { return name_; }

protected:
    const std::string name_;
};


template<typename DataT>
class Publisher
{
public:
    using Ptr = std::shared_ptr<Publisher<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

public:
    explicit Publisher(const std::string& name) : name_(name){}
    virtual ~Publisher() = default;

    virtual void publish(const DataPtr& data) = 0;

    inline std::string name() const { return name_; }
protected:
    const std::string name_;
};

template<typename DataT>
class Subscriber
{
public:
    using Ptr = std::shared_ptr<Subscriber<DataT>>;

    using DataPtr = ShmSharedPtr<DataT>;
    using ConstDataPtr = const DataPtr;

    using Callback = std::function< void(ConstDataPtr) >;

public:
    explicit Subscriber(const std::string& name) : name_(name){}
    virtual ~Subscriber() = default;

    virtual bool is_online() = 0;
    virtual bool is_update() = 0;

    inline std::string name() const { return name_; }
protected:
    const std::string name_;
};

template<typename ReqT, typename AckT>
class Service
{
public:
    using Ptr = std::shared_ptr<Service<ReqT, AckT>>;

    using ReqType = ReqT;
    using AckType = AckT;
    using ReqPtr = ShmSharedPtr<ReqType>;
    using AckPtr = ShmSharedPtr<AckType>;
    using ConstReqPtr = const ReqPtr;
    using ConstAckPtr = const AckPtr;
    using Callback = std::function< bool(ConstReqPtr&, AckPtr&) >;

public:
    explicit Service(const std::string& name) : name_(name){}
    virtual ~Service() = default;

    virtual bool is_online() = 0;
    virtual bool is_update() = 0;

    inline std::string name() const { return name_; }
protected:
    const std::string name_;
};

template<typename ReqT, typename AckT>
class Client
{
public:
    using Ptr = std::shared_ptr<Client<ReqT, AckT>>;

    using ReqType = ReqT;
    using AckType = AckT;
    using ReqPtr = ShmSharedPtr<ReqType>;
    using AckPtr = ShmSharedPtr<AckType>;
    using ConstReqPtr = const ReqPtr;
    using ConstAckPtr = const AckPtr;
    using Callback = std::function< bool(ConstReqPtr&, AckPtr&) >;
public:
    explicit Client(const std::string& name) : name_(name){}
    virtual ~Client() = default;

    virtual bool is_online() = 0;
    virtual bool is_update() = 0;

    virtual bool call(ConstReqPtr& req, AckPtr& ack) = 0;

    inline std::string name() const { return name_; }
protected:
    const std::string name_;
};

} // namespace coin::data
