/**
 * @file coin-data.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <coin-data/communicator.hpp>
#include <coin-data/communicator_type.hpp>

#include <coin-commons/utils/utils.hpp>

namespace coin::data
{

int init(int argc, char *argv[], const std::string& name = "");
template<typename T>
[[nodiscard]] typename Writer<T>::Ptr writer(const std::string& name)
{
    return Communicator::writer<T>(name);
}
template<typename T>
[[nodiscard]] typename Reader<T>::Ptr reader(const std::string& name)
{
    return Communicator::reader<T>(name);
}
template<typename T>
[[nodiscard]] typename Publisher<T>::Ptr publisher(const std::string& name)
{
    return Communicator::publisher<T>(name);
}
template<typename T>
[[nodiscard]] typename Subscriber<T>::Ptr subscriber(const std::string& name, const typename Subscriber<T>::Callback& cb)
{
    return Communicator::subscriber<T>(name, cb);
}
template<typename ReqT, typename AckT>
[[nodiscard]] typename Service<ReqT, AckT>::Ptr service(const std::string& name, const typename Service<ReqT, AckT>::Callback& cb)
{
    return Communicator::service<ReqT, AckT>(name, cb);
}
template<typename ReqT, typename AckT>
[[nodiscard]] typename Client<ReqT, AckT>::Ptr client(const std::string& name)
{
    return Communicator::client<ReqT, AckT>(name);
}
} // namespace coin::data
