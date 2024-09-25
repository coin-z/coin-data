/**
 * @file ipcs.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <coin-data/local/impl/ipcs.hpp>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <coin-commons/utils/utils.hpp>

namespace coin::data
{
Shm::Shm(const std::string& key, size_t size, const void* addr)
  : shm_(std::make_unique<coin::ipc::Shm>(key, size, addr))
{
    int ret = 0;
    // check whether shared memory exists, 
    // if exists and not mounted, destroy it
    if(shm_->num_of_attach() == 0)
    {
        shm_->destroy();
    }

    // create shared memory
    ret = shm_->create(); if(ret != 0) { /* Don't Need Check */}
    ret = shm_->attach(); if(ret != 0) coin::Print::error("attach shared memory failed");

    // if create memory is not match to required memory
    // destroy shared memory and throw runtime exception
    if(addr != nullptr && shm_->addr() != addr)
    {
        coin::Print::error("create memory is not match({}:{})", shm_->addr(), addr);
        shm_->destroy();
        throw std::runtime_error("create memory is not match");
    }

    #if 0
    // buddy memory check
    buddy_memory_check(*addr, size);
    #endif
}
Shm::~Shm()
{
    shm_->detach();
    if(shm_->is_free())
    {
        shm_->destroy();
    }
    return;
}

void* Shm::addr()
{
    if(shm_)
    {
        return shm_->addr();
    }
    return nullptr;
}
size_t Shm::size()
{
    if(shm_)
    {
        return shm_->size();
    }
    return 0;
}
const size_t Shm::num_of_attach() const
{
    if(shm_)
    {
        return shm_->num_of_attach();
    }
    return 0;
}
const bool Shm::is_free() const
{
    if(shm_)
    {
        return shm_->is_free();
    }
    return true;
}
} // namespace coin::data
