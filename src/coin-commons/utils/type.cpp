/**
 * @file type.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "utils/type.hpp"
#include <cxxabi.h>
#include <iostream>

namespace coin
{
std::string type_name(const std::type_info &type)
{
    std::string name("");
    int status = 0;
    char* demangled_name = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    if (demangled_name != nullptr) {
        name = std::string(demangled_name);
        std::free(demangled_name);
    } else {
        std::cerr << "Failed to demangle name: " << status << std::endl;
    }
    return name;
}
std::string type_name(const std::any &any)
{
    return type_name(any.type());
}
} // namespace coin::type
