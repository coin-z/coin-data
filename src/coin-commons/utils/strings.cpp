/**
 * @file strings.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <coin-commons/utils/strings.hpp>

namespace coin::strings
{
std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> result;
    std::string_view sv = str;
    std::string::size_type pos = 0;
    pos = sv.find_first_not_of(delimiter, 0);
    while (pos != std::string::npos)
    {
        std::string::size_type end = sv.find(delimiter, pos);
        if (end == std::string::npos)
        {
            result.push_back(std::string(sv.substr(pos)));
            break;
        }
        result.push_back(std::string(sv.substr(pos, end - pos)));
        pos = sv.find_first_not_of(delimiter, end);
    }
    return std::move(result);
}
std::string remove_leading_chars(const std::string& str, const std::string& chars)
{
    auto pos = str.find_first_not_of(chars);
    if(pos == std::string::npos)
    {
        return str;
    }
    return str.substr(pos);
}
std::string_view remove_leading_chars(const std::string_view& str, const std::string& chars)
{
    auto pos = str.find_first_not_of(chars);
    if(pos == std::string_view::npos)
    {
        return str;
    }
    return str.substr(pos);
}
} // namespace coin::strings
