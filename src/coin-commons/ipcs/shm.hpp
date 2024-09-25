/**
 * @file shm.hpp
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

namespace coin::ipc
{
class Shm
{
public:
    Shm(const std::string& key_file, const size_t size, const void* addr = nullptr);
    ~Shm();

    int create();
    int destroy();
    int attach();
    int detach();
    int num_of_attach();
    
    bool is_free();

    void* addr();
    size_t size();

    inline const std::string key_file() const { return key_file_; }

private:
    key_t get_key_(const std::string& key_file);

private:
    const std::string key_file_;
    const key_t key_;
    const void* req_addr_;
    void* addr_;
    const size_t size_;
}; // class Shm
} // namespace coin
