/**
 * @file shm_manager.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <unistd.h>
#include <sys/types.h>
#include <utility>
#include <filesystem>

#include <coin-data/local/impl/shm_manager.hpp>
#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/crc32.hpp>
#include "shm_manager.hpp"

namespace coin::data
{
constexpr char shm_root_path[] = "/dev/shm/coin/";
constexpr std::size_t ctrl_mem_size = coin::data::__inner::ShmMemory::PAGE_SIZE;         // 4k
constexpr std::size_t data_mem_size = coin::data::__inner::ShmMemory::PAGE_SIZE * 25600; // 100MB

std::string ShmManager::get_node_root_path(const std::string &node)
{
    return (shm_root_path + node);
}
std::string ShmManager::get_ctl_key_file(const std::string &node, const std::string &key)
{
    return (shm_root_path + node + "/" + key);
}
std::string ShmManager::get_data_key_file(const std::string& node, const std::string& key)
{
    return (shm_root_path + node + "/" + key + ".mem.data");
}
ShmManager::ShmManager(const std::string& node)
  : self_pid_(getpid())
  , key_path_(shm_root_path + node)
  , key_file_(get_ctl_key_file(node, std::to_string(self_pid_)))
  , data_file_(get_data_key_file(node, std::to_string(self_pid_)))
{
    // check key_path_ exists, if not, create it
    if(!std::filesystem::exists(key_path_))
    {
        std::filesystem::create_directories(key_path_);
    }

    // make shm_mem_
    shm_mem_ = std::make_unique<__inner::ShmMemory>(
        std::pair<const std::string, const std::size_t>(key_file_, ctrl_mem_size),
        std::pair<const std::string, const std::size_t>(data_file_, data_mem_size)
    );
    shm_mem_->initialized();

    coin::Print::info("ctrl mem addr: {:X}, data mem addr: {:X}", (size_t)shm_mem_->ctl_addr(), (size_t)shm_mem_->data_addr());
}
ShmManager::ShmManager(const std::string& node, const pid_t pid, void* ctl_addr, void* data_addr)
  : self_pid_(pid)
  , key_path_(shm_root_path + node)
  , key_file_(get_ctl_key_file(node, std::to_string(self_pid_)))
  , data_file_(get_data_key_file(node, std::to_string(self_pid_)))
{
    // check key_path_ exists, if not, throw exception out
    if(!std::filesystem::exists(key_path_))
    {
        coin::Print::error("node path not exists: {}", key_path_);
        throw std::runtime_error("node path not exists");
    }
    // make shm_mem_
    shm_mem_ = std::make_unique<__inner::ShmMemory>(
            std::tuple<const std::string, const std::size_t, void*> (key_file_,  ctrl_mem_size, ctl_addr),
            std::tuple<const std::string, const std::size_t, void*> (data_file_, data_mem_size, data_addr)
        );
}
ShmManager::~ShmManager()
{

}
__inner::ShmMemory& ShmManager::mem()
{
    return *shm_mem_;
}
} // namespace coin::data
