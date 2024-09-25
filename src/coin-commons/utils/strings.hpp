/**
 * @file strings.hpp
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
#include <vector>

namespace coin::strings
{
/**
 * @brief split string to vector by delimiter
 */
std::vector<std::string> split(const std::string& str, const std::string& delimiter = " ");

/**
 * @brief remove leading chars
 */
std::string remove_leading_chars(const std::string& str, const std::string& chars);
std::string_view remove_leading_chars(const std::string_view& str, const std::string& chars);
} // namespace coin::strings
