/**
 * @file anydata_impl.hpp
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

#include <functional>

#include <coin-commons/utils/ret.hpp>
#include <coin-commons/utils/type.hpp>

namespace coin
{
//////////////////////////////////////////////////////////////////////////////
template<typename ParserT>
class AnyDataNode
{

public:
    AnyDataNode(const std::string& key);
    virtual ~AnyDataNode();

    virtual size_t size() const = 0;
    virtual size_t encode(typename ParserT::Node& parser) const = 0;
    virtual size_t decode(const typename ParserT::Node& parser) = 0;
    virtual std::string to_string() const = 0;
    virtual RetBool from_string(const std::string& str) = 0;

    AnyDataNode& operator[](const std::string& key);

    virtual inline const bool is_node() const = 0;

    AnyDataNode& add(const NodePtr<ParserT>& value);

    AnyDataNode& add(const std::string& key, const std::initializer_list< NodePtr<ParserT> >& values);
    
    template<typename T>
    AnyDataNode& add(const std::string& key, const T& value) {
        return add(
            std::make_shared<AnyDataItem<T, ParserT>> (key, value)
        );
    }

    void walking(const std::function<void(const AnyDataNode&)>& func) const;

    // 用于访问数据方法
    template<typename T>
    const std::shared_ptr< T > access() const {
        return std::any_cast< std::shared_ptr< T > >(__m_value);
    }
    template<typename T>
    std::shared_ptr< T > access() {
        return std::any_cast< std::shared_ptr< T >>(__m_value);
    }

    template<typename T>
    T as() const { return *std::any_cast<std::shared_ptr<T>>(__m_value); }

    template<typename T>
    inline T get(const std::string& key, const T& value)
    {
        // 检查 Node
        if(not is_node())
        {
            std::stringstream ss;
            ss << "AnyDataNode::operator[] : not a node, key = " << key;
            throw std::runtime_error(ss.str());
        }

        // 拆分 key 的层级
        const char mark = '.';
        std::string::size_type pos1 = 0, pos2 = 0;
        pos2 = key.find(mark);

        auto cNode = std::any_cast<NodeMapPtr<ParserT>>( __m_value );
        
        while(std::string::npos != pos2)
        {
            auto k = key.substr(pos1, pos2-pos1);

            // 检查 key 是否存在，不存在则插入一个空节点
            auto item = cNode->find(k);
            if(item == cNode->end())
            {
                std::any_cast<NodeMapPtr<ParserT>>( __m_value )->operator[] (key) = \
                    std::make_shared<AnyDataTable<ParserT>>(key);
            }

            auto node = cNode->operator[](k);
            if(node->is_node())
            {
                cNode = std::any_cast<NodeMapPtr<ParserT>>( node->__m_value );
            }
            else
            {
                std::stringstream ss;
                ss << "AnyDataNode::operator[] : not a node, key = " << key;
                throw std::runtime_error(ss.str());
            }

            pos1 = pos2 + 1;
            pos2 = key.find(mark, pos1);
        }

        auto k = key.substr(pos1);
        auto ret = cNode->find(k);
        if(ret == cNode->end())
        {
            for(auto& i : *cNode)
            {
                std::cout << i.first << std::endl;
            }
            cNode->operator[] (k) = std::make_shared<AnyDataItem<T, ParserT>>(k, value);
        }
        return (cNode->operator[] (k))->template as<T>();
    }

    template<typename T>
    inline void set(const std::string& key, const T& value)
    {
        // 检查 Node
        if(not is_node())
        {
            std::stringstream ss;
            ss << "AnyDataNode::operator[] : not a node, key = " << key;
            throw std::runtime_error(ss.str());
        }

        // 拆分 key 的层级
        const char mark = '.';
        std::string::size_type pos1 = 0, pos2 = 0;
        pos2 = key.find(mark);

        auto cNode = std::any_cast<NodeMapPtr<ParserT>>( __m_value );
        
        while(std::string::npos != pos2)
        {
            auto k = key.substr(pos1, pos2-pos1);

            // 检查 key 是否存在，不存在则插入一个空节点
            auto item = cNode->find(k);
            if(item == cNode->end())
            {
                std::any_cast<NodeMapPtr<ParserT>>( __m_value )->operator[] (key) = \
                    std::make_shared<AnyDataTable<ParserT>>(key);
            }

            auto node = cNode->operator[](k);
            if(node->is_node())
            {
                cNode = std::any_cast<NodeMapPtr<ParserT>>( node->__m_value );
            }
            else
            {
                std::stringstream ss;
                ss << "AnyDataNode::operator[] : not a node, key = " << key;
                throw std::runtime_error(ss.str());
            }

            pos1 = pos2 + 1;
            pos2 = key.find(mark, pos1);
        }

        auto k = key.substr(pos1);
        auto ret = cNode->find(k);
        if(ret == cNode->end())
        {
            cNode->operator[] (k) = std::make_shared<AnyDataItem<T, ParserT>>(k, value);
        }
        else
        {
            ret->second = std::make_shared<AnyDataItem<T, ParserT>>(k, value);
        }
    }

    inline const std::string& key() const { return __m_key; }
    inline const std::string& value() const { return to_string(); }

    inline const std::string type() const { return std::string( __m_value.type().name() ); }

protected:
    std::string __m_key;
public:
    std::any __m_value;

};

//////////////////////////////////////////////////////////////////////////////

template<typename ParserT>
class AnyDataTable final : public AnyDataNode<ParserT>
{

public:
    AnyDataTable();
    AnyDataTable(const std::string& key);
    AnyDataTable(const std::string& key, \
                const std::initializer_list< NodePtr<ParserT> >& values);
    virtual ~AnyDataTable() override;

    std::size_t size() const override;

    std::size_t encode(typename ParserT::Node& parser) const override;

    std::size_t decode(const typename ParserT::Node& parser) override;

    std::string to_string() const override;

    RetBool from_string(const std::string& str) override;

    virtual inline const bool is_node() const override { return true; }

};

//////////////////////////////////////////////////////////////////////////////

template<typename T, typename ParserT>
class AnyDataItem final : public AnyDataNode<ParserT>
{
public:
    using ValueType = T;
    using ValuePtr = std::shared_ptr<ValueType>;

public:
    AnyDataItem(const std::string& key, const T& value);
    virtual ~AnyDataItem() override;

    operator T& () const {
        return *(std::any_cast<ValuePtr>(AnyDataNode<ParserT>::__m_value));
    }

    std::size_t size() const override;

    std::size_t encode(typename ParserT::Node& parser) const override;

    std::size_t decode(const typename ParserT::Node& parser) override;

    std::string to_string() const override;

    RetBool from_string(const std::string& str) override;

    virtual inline const bool is_node() const override { return false; }


};

//////////////////////////////////////////////////////////////////////////////

template<typename ParserT>
AnyDataNode<ParserT>::AnyDataNode(const std::string &key) : __m_key(key)
{
}

template<typename ParserT>
AnyDataNode<ParserT>::~AnyDataNode()
{
}

template<typename ParserT>
AnyDataNode<ParserT> &AnyDataNode<ParserT>::operator[](const std::string &key)
{
    // 检查 Node
    if(not is_node())
    {
        std::stringstream ss;
        ss << "AnyDataNode::operator[] : not a node, key = " << key;
        throw std::runtime_error(ss.str());
    }

    // 拆分 key 的层级
    const char mark = '.';
    std::string::size_type pos1 = 0, pos2 = 0;
    pos2 = key.find(mark);

    auto cNode = std::any_cast<NodeMapPtr<ParserT>>( __m_value );
    
    while(std::string::npos != pos2)
    {
        auto k = key.substr(pos1, pos2-pos1);

        auto node = cNode->operator[](k);
        if(node->is_node())
        {
            cNode = std::any_cast<NodeMapPtr<ParserT>>( node->__m_value );
        }
        else
        {
            std::stringstream ss;
            ss << "AnyDataNode::operator[] : not a node, key = " << key;
            throw std::runtime_error(ss.str());
        }

        pos1 = pos2 + 1;
        pos2 = key.find(mark, pos1);
    }

    auto k = key.substr(pos1);
    auto ret = cNode->find(k);
    if(ret == cNode->end())
    {
        std::cout << "AnyDataNode::operator[] : not found, key = " << key << ", insert it" << std::endl;
        throw std::runtime_error("AnyDataNode::operator[] : not found");
    }
    return *(cNode->operator[] (k));
}

template<typename ParserT>
AnyDataNode<ParserT> &AnyDataNode<ParserT>::add(const NodePtr<ParserT> &value)
{

    if(not is_node())
    {
        std::stringstream ss;
        ss << "AnyDataNode::add() : not a node, key = " << key() \
           << ", type = " << coin::type_name(*this) << ", " \
           << coin::type_name<AnyDataTable<ParserT>>();
        throw std::runtime_error(ss.str());
    }

    const auto& nodeMap = std::any_cast<NodeMapPtr<ParserT>>( __m_value );

    if(nodeMap->find(value->key()) != nodeMap->end())
    {
        throw std::runtime_error("AnyDataNode::add() : key already exists");
    }

    std::any_cast<NodeMapPtr<ParserT>>( __m_value )->operator[] (value->key()) = value;

    return *this;

}

template<typename ParserT>
AnyDataNode<ParserT> &AnyDataNode<ParserT>::add(const std::string &key, const std::initializer_list<NodePtr<ParserT>> &values)
{

    if(not is_node())
    {
        std::stringstream ss;
        ss << "AnyDataNode::add() : not a node, key = " << key;
        throw std::runtime_error(ss.str());
    }

    const auto& nodeMap = std::any_cast<NodeMapPtr<ParserT>>( __m_value );

    if(nodeMap->find(key) != nodeMap->end())
    {
        throw std::runtime_error("AnyDataNode::add() : key already exists");
    }

    std::any_cast<NodeMapPtr<ParserT>>( __m_value )->operator[] (key) = \
        std::make_shared<AnyDataTable<ParserT>>(key, values);

    return *this;

}

template <typename ParserT>
inline void AnyDataNode<ParserT>::walking (const std::function<void(const AnyDataNode &)> &func) const
{
    
    {
        func(*this);
    }
    if(is_node())
    {
        const auto& nodeMap = std::any_cast<NodeMapPtr<ParserT>>( __m_value );
        for(const auto& [k, v] : *nodeMap)
        {

            if(v->is_node())
            {
                v->walking(func);
            }
            else
            {
                func(*v);
            }
        }
    }

}

//////////////////////////////////////////////////////////////////////////////

template <typename T, typename ParserT>
inline AnyDataItem<T, ParserT>::AnyDataItem(const std::string &key, const T &value) : AnyDataNode<ParserT>(key)
{
    AnyDataNode<ParserT>::__m_value = std::make_shared<T>(value);
}

template <typename T, typename ParserT>
inline AnyDataItem<T, ParserT>::~AnyDataItem()
{
}

template <typename T, typename ParserT>
std::size_t AnyDataItem<T, ParserT>::size() const
{
    return coin::anydata::AnyDataAdapter<T, ParserT>::size(*std::any_cast<std::shared_ptr<T>>( AnyDataNode<ParserT>::__m_value ));
}

template <typename T, typename ParserT>
std::size_t AnyDataItem<T, ParserT>::encode(typename ParserT::Node& parser) const {

    coin::anydata::AnyDataAdapter<T, ParserT>::encode(
        AnyDataNode<ParserT>::key(), 
        *std::any_cast<std::shared_ptr<T>>( AnyDataNode<ParserT>::__m_value ), 
        parser);
    return size();
}

template <typename T, typename ParserT>
std::size_t AnyDataItem<T, ParserT>::decode(const typename ParserT::Node& parser) {
    coin::anydata::AnyDataAdapter<T, ParserT>::decode(
        parser,
        AnyDataNode<ParserT>::key(), 
        *std::any_cast<std::shared_ptr<T>>( AnyDataNode<ParserT>::__m_value 
        ));
    return size();
}

template <typename T, typename ParserT>
std::string AnyDataItem<T, ParserT>::to_string() const {
    std::string str;
    coin::anydata::AnyDataAdapter<T, ParserT>::to_string( *std::any_cast<std::shared_ptr<T>>( AnyDataNode<ParserT>::__m_value ), str);
    return str;
}

template <typename T, typename ParserT>
RetBool AnyDataItem<T, ParserT>::from_string(const std::string& str) {
    return 
        coin::anydata::AnyDataAdapter<T, ParserT>::from_string( str, *std::any_cast<std::shared_ptr<T>>( AnyDataNode<ParserT>::__m_value ) );
}


//////////////////////////////////////////////////////////////////////////

template<typename ParserT>
AnyDataTable<ParserT>::AnyDataTable() : AnyDataNode<ParserT>("")
{
    AnyDataNode<ParserT>::__m_value = std::make_shared<NodeMap<ParserT>>();
}

template<typename ParserT>
AnyDataTable<ParserT>::AnyDataTable(const std::string &key) : AnyDataNode<ParserT>(key)
{
    AnyDataNode<ParserT>::__m_value = std::make_shared<NodeMap<ParserT>>();
}

template<typename ParserT>
AnyDataTable<ParserT>::AnyDataTable(const std::string &key, \
                    const std::initializer_list< NodePtr<ParserT> > &values)
  : AnyDataNode<ParserT>(key)
{
    AnyDataNode<ParserT>::__m_value = std::make_shared<NodeMap<ParserT>>();
    for (auto &item : values)
    {
        std::any_cast<NodeMapPtr<ParserT>>(AnyDataNode<ParserT>::__m_value)->operator[](item->key()) = item;
    }
}

template<typename ParserT>
AnyDataTable<ParserT>::~AnyDataTable()
{
}

template<typename ParserT>
std::size_t AnyDataTable<ParserT>::size() const
{
    return std::size_t();
}

template<typename ParserT>
std::size_t AnyDataTable<ParserT>::encode(typename ParserT::Node& parser) const
{
    // insert table
    coin::anydata::AnyDataAdapter<AnyDataTable<ParserT>, ParserT>::encode(
        AnyDataNode<ParserT>::key(), 
        std::any_cast<NodeMapPtr<ParserT>>( AnyDataNode<ParserT>::__m_value ),
        parser
    );

    // encode all items
    for (auto &item : *std::any_cast<NodeMapPtr<ParserT>>(AnyDataNode<ParserT>::__m_value))
    {
        item.second->encode(
            (*static_cast<typename ParserT::Table*>( &parser )).at(AnyDataNode<ParserT>::key())
        );
    }

    return size();
}

template<typename ParserT>
std::size_t AnyDataTable<ParserT>::decode(const typename ParserT::Node& parser)
{
    auto ret = coin::anydata::AnyDataAdapter<AnyDataTable<ParserT>, ParserT>::decode(
        parser,
        AnyDataNode<ParserT>::key(), 
        std::any_cast<NodeMapPtr<ParserT>>( AnyDataNode<ParserT>::__m_value )
    );

    if(ret == 0)
    {
        return 0;
    }
    
    // decode all items
    for (auto &item : *std::any_cast<NodeMapPtr<ParserT>>(AnyDataNode<ParserT>::__m_value))
    {
        auto& node = static_cast<const typename ParserT::Table*>( &parser ) -> at(AnyDataNode<ParserT>::key());
        item.second->decode(node);
    }
    return size();
}

template<typename ParserT>
std::string AnyDataTable<ParserT>::to_string() const
{
    return "";
}

template<typename ParserT>
RetBool AnyDataTable<ParserT>::from_string(const std::string &str)
{
    return RetBool();
}

} // namespace coin
