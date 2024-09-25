/**
 * @file type.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <any>
#include <coin-commons/utils/ret.hpp>

namespace coin
{
/**
 * @brief get type name by type info
 * @example
 *     auto name = type_name(typeid(int));
 */
std::string type_name(const std::type_info& type);

/**
 * @brief get type name by any
 * @example
 *     any num = 1;
 *     auto name = type_name(num);
 */
std::string type_name(const std::any& any);

/**
 * @brief get type name by variable
 * @example
 *     int a = 1;
 *     auto name = type_name(a);
 */
template<typename T>
std::string type_name(const T& t)
{
    return type_name(typeid(t));
}

/**
 * @brief get type name by type
 * @example
 *     auto name = type_name<int>();
 */
template<typename T>
std::string type_name()
{
    return type_name(typeid(T));
}
} // namespace coin::type
