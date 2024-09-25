/**
 * @file ipcs.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __IPCS_H__
#define __IPCS_H__

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <coin-commons/ipcs/shm.hpp>


namespace coin::data
{
class Shm
{
public:
    Shm(const std::string& key, size_t size, const void* addr = nullptr);
    ~Shm();

    void* addr();
    size_t size();
    const size_t num_of_attach() const;
    const bool is_free() const;

    inline const std::string key_file() const { return shm_->key_file(); }

private:
    std::unique_ptr<coin::ipc::Shm> shm_;
}; // class Shm
}
#endif // __IPCS_H__
