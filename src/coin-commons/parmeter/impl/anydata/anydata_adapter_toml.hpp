/**
 * @file anydata_adapter_toml.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <any>
#include <typeindex>
#include <sstream>
#include <type_traits>
#include <coin-commons/utils/ret.hpp>
#include <coin-commons/utils/type.hpp>

#include <toml.hpp>
#include <coin-commons/parmeter/impl/anydata/anydata_adapter.hpp>

#include <iostream>

namespace coin::anydata
{

struct TomlParserType
{
    using Node = ::toml::node;
    using Table = ::toml::table;
    using Array = ::toml::array;
    template<typename T>
    using Item = ::toml::value<T>;
};

template<typename T>
struct AnyDataAdapter<T, TomlParserType>
{
static std::size_t size(const typename std::enable_if<std::is_pod<T>::value, T>::type& value)
{
    return sizeof(T);
}

static std::size_t encode(
    const std::string& key,
    const typename std::enable_if<std::is_pod<T>::value, T>::type& value,
    TomlParserType::Node& parser
    )
{
    parser.ref<TomlParserType::Table>().insert_or_assign(key, value);
    return size(value);
}

static std::size_t decode(const TomlParserType::Node& parser,
    const std::string& key, 
    typename std::enable_if<std::is_pod<T>::value, T>::type& value
    )
{
    // 检查是否存在
    if(!parser.ref<TomlParserType::Table>().contains(key))
    {
        return 0;
    }
    value = parser.ref<TomlParserType::Table>()[key].value_or<T>(T());
    return 0;
}

static RetBool to_string(const typename std::enable_if<std::is_pod<T>::value, T>::type& data, std::string& str)
{
    str = std::to_string(data);
    return true;
}

static RetBool from_string(const std::string& str, typename std::enable_if<std::is_pod<T>::value, T>::type& data)
{
    std::stringstream ss;
    ss << str;
    ss >> data;
    return true;
}
}; // struct AnyDataAdapter

template<>
struct AnyDataAdapter<std::string, TomlParserType>
{
static std::size_t size(const std::string& value)
{
    return value.size();
}

static std::size_t encode(
    const std::string& key,
    const std::string& value,
    TomlParserType::Node& parser
    )
{
    parser.ref<TomlParserType::Table>().insert_or_assign(key, value);
    return size(value);
}

static std::size_t decode(const TomlParserType::Node& parser,
    const std::string& key, 
    std::string& value
    )
{

    // 检查是否存在
    if(!parser.ref<TomlParserType::Table>().contains(key))
    {
        return 0;
    }

    value = parser.ref<TomlParserType::Table>()[key].value_or<std::string>("");
    return size(value);
}

static RetBool to_string(const std::string& data, std::string& str)
{
    str = data;
    return true;
}

static RetBool from_string(const std::string& str, std::string& data)
{
    data = str;
    return true;
}
};


template<typename T>
struct AnyDataAdapter<::std::vector<T>, TomlParserType>
{
static std::size_t size(const ::std::vector<T>& value)
{
    return value.size();
}

static std::size_t encode(
    const std::string& key,
    const ::std::vector<T>& value,
    TomlParserType::Node& parser
    )
{
    TomlParserType::Array arr;
    for(auto v : value)
    {
        arr.push_back(v);
    }
    parser.ref<TomlParserType::Table>().insert_or_assign(key, arr);
    return size(value);
}

static std::size_t decode(const TomlParserType::Node& parser,
    const std::string& key, 
    ::std::vector<T>& value
    )
{

    // 检查是否存在
    if(!parser.ref<TomlParserType::Table>().contains(key))
    {
        return 0;
    }

    const toml::array* arr = parser.ref<const TomlParserType::Table>()[key].as_array();
    value.clear();
    for(size_t idx = 0; idx < arr->size(); idx++)
    {
        value.push_back(arr->get(idx)->value_or<T>(T()));
    }

    return size(value);
}

static RetBool to_string(const ::std::vector<T>& data, std::string& str)
{
    std::string s;
    for(auto v : data)
    {
        AnyDataAdapter<T, TomlParserType>::to_string(v, s);
        str += s;
        str += ",";
    }
    str = "[" + str.substr(0, str.size() - 1) + "]";
    return true;
}

static RetBool from_string(const std::string& str, ::std::vector<T>& data)
{
    std::stringstream ss;
    ss << str.substr(1, str.size() - 2);

    while(!ss.eof())
    {
        std::string d;
        std::getline(ss, d, ',');

        T v;
        AnyDataAdapter<T, TomlParserType>::from_string(d, v);
        data.push_back(v);
    }

    return true;
}
};


template<>
struct AnyDataAdapter<AnyDataTable<TomlParserType>, TomlParserType>
{

static std::size_t size(const NodeMapPtr<TomlParserType>& value)
{
    return 1;
}

static std::size_t encode(
    const std::string& key,
    const NodeMapPtr<TomlParserType>& value,
    TomlParserType::Node& parser
    )
{
    parser.ref<TomlParserType::Table>().insert_or_assign(key, TomlParserType::Table());
    return size(value);
}

static std::size_t decode(const TomlParserType::Node& parser,
    const std::string& key, 
    const NodeMapPtr<TomlParserType>& value
    )
{
    // 检查是否存在
    if(!parser.ref<TomlParserType::Table>().contains(key))
    {
        return 0;
    }

    return size(value);
}

static RetBool to_string(const AnyDataTable<TomlParserType>& data, std::string& str)
{
    return true;
}

static RetBool from_string(const std::string& str, AnyDataTable<TomlParserType>& data)
{
    return true;
}

};
} // namespace coin::anydata

//////////////////////////////////////////////////////////////////////////////
#include <coin-commons/parmeter/impl/anydata/anydata_impl.hpp>
namespace coin
{
namespace anydata
{
namespace toml
{
inline NodePtr<TomlParserType> Node(const std::string& key,\
    const std::initializer_list< NodePtr<TomlParserType> >& values)
{
    return std::make_shared<AnyDataTable<TomlParserType>>(key, values);
}

template<typename T>
inline NodePtr<TomlParserType> Item(const std::string& key, const T& value)
{
    return std::make_shared<AnyDataItem<T, TomlParserType>>(key, value);
}
} // namespace toml
} // namespace anydata

using TomlAnyDataNode = AnyDataNode<anydata::TomlParserType>;
using TomlAnyDataTable = AnyDataTable<anydata::TomlParserType>;
template<typename T>
using TomlAnyDataItem = AnyDataItem<T, anydata::TomlParserType>;

} // namespace coin
