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
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <utility>
#include <fstream>
#include <filesystem>

#include <coin-data/local/impl/shm_manager.hpp>
#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/crc32.hpp>
#include "shm_manager.hpp"

namespace coin::data
{
static std::string shm_root_path = "/dev/shm/coin/";
static const std::string mem_pool_path = "/dev/shm/coin-memory-pool/";
static const std::size_t ctrl_mem_size = coin::data::__inner::ShmMemory::page_size();         // 4k
static const std::size_t data_mem_size = coin::data::__inner::ShmMemory::page_size() * 25600; // 100MB
static const uint64_t memory_base_addr = 0x600007ffb000;//0x7FFFF6FFA000;

constexpr std::size_t byte_1M = (1024 * 1024);

static uint64_t shm_addr = 0;
static uint64_t data_addr = 0;

static bool is_memory_block_valid(void* addr, const uint64_t size)
{
    // if(mmap(addr, coin::data::__inner::ShmMemory::page_size(), PROT_NONE, MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
    // {
    //     return false;
    // }
    // munmap((void*)addr, size);
    return true;
}

std::pair<uint64_t, uint64_t> get_aviailable_addr(const std::string& node)
{
    if(not std::filesystem::exists(mem_pool_path))
    {
        std::filesystem::create_directories(mem_pool_path);
    }
    const std::string lock_file = mem_pool_path + "/lock";
    if(not std::filesystem::exists(lock_file))
    {
        std::ofstream ofs(lock_file);
        ofs.close();
    }

    coin::FileLock lock(lock_file);
    coin::FileLockGuard lock_guard(lock);

    std::pair<uint64_t, uint64_t> ret;

    size_t cnt = 0;
    size_t blank_size = 4 * coin::data::__inner::ShmMemory::page_size();
    size_t block_size = data_mem_size + ctrl_mem_size + blank_size;
    std::string mem_info;

    if(!std::filesystem::exists(mem_pool_path))
    {
        std::filesystem::create_directories(mem_pool_path);
    }

    while(true)
    {
        ret.first = memory_base_addr + (cnt * block_size);
        ret.second = ret.first + ctrl_mem_size + blank_size;
        mem_info = fmt::format("{:X}.{:X}", ret.first, ret.second);

        if(std::filesystem::exists(mem_pool_path + mem_info))
        {
            char link_target[512];
            int r = readlink((mem_pool_path + mem_info).c_str(), link_target, sizeof(link_target));
            link_target[r] = 0;

            // check the owener of this memory is alive.
            std::ifstream ifs(mem_pool_path + mem_info + "/pid");
            pid_t pid;
            ifs >> pid;

            coin::Print::debug("memory pool link target: {}, pid: {}", link_target, pid);
            // check shm memory wheather used by other process, if not, release it.
            std::string shm_key_file = mem_pool_path + mem_info + "/" + std::to_string(pid) + "/shm/";
            coin::data::Shm::check_and_remove(shm_key_file + std::to_string(pid));
            coin::data::Shm::check_and_remove(shm_key_file + std::to_string(pid) + ".mem.data");

            if(pid > 0 and kill(pid, 0) != 0)
            {
                coin::Print::warn("pid {} is died.", pid);

                if(r >= 0)
                {
                    coin::Print::warn("remove directory: {}", link_target);
                    std::filesystem::remove_all(link_target);
                }

                unlink((mem_pool_path + mem_info).c_str());
                break;
            }
            if(pid == getpid())
            {
                break;
            }
            cnt++;
        }
        else
        {
            break;
        }
    }
    symlink(node.c_str(), (mem_pool_path + mem_info).c_str());

    return ret;
}

std::string ShmManager::get_root()
{
    return shm_root_path;
}

void set_node_shm_root_path(const std::string& path)
{
    shm_root_path = "/dev/shm/coin/";
}

std::string ShmManager::get_node_root_path(const std::string &node, const std::string& key)
{
    return (shm_root_path + node + "/" + key + "/shm/");
}
std::string ShmManager::get_ctl_key_file(const std::string &node, const std::string &key)
{
    return (shm_root_path + node + "/" + key + "/shm/" + key);
}
std::string ShmManager::get_data_key_file(const std::string& node, const std::string& key)
{
    return (shm_root_path + node + "/" + key + "/shm/" + key + ".mem.data");
}
void ShmManager::update_shm_addr(const std::string& node)
{
    auto node_path = (shm_root_path + node);
    auto available_addr = get_aviailable_addr(node_path);
    shm_addr = available_addr.first;
    data_addr = available_addr.second;
}
ShmManager::ShmManager(const std::string &node)
    : self_pid_(getpid()), key_path_(shm_root_path + node), key_file_(get_ctl_key_file(node, std::to_string(self_pid_))), data_file_(get_data_key_file(node, std::to_string(self_pid_)))
{
    // check key_path_ exists, if not, create it
    if(!std::filesystem::exists(key_path_))
    {
        std::filesystem::create_directories(key_path_);
    }

    if(not is_memory_block_valid((void*)shm_addr, ctrl_mem_size))
    {
        coin::Print::error("shm memory is not valid.");
        exit(1);
    }
    if(not is_memory_block_valid((void*)data_addr, data_mem_size))
    {
        coin::Print::error("data memory is not valid.");
        exit(1);
    }

    // make shm_mem_
    shm_mem_ = std::make_unique<__inner::ShmMemory>(
        std::tuple<const std::string, const std::size_t, void*>(key_file_, ctrl_mem_size, (void*)shm_addr),
        std::tuple<const std::string, const std::size_t, void*>(data_file_, data_mem_size, (void*)data_addr)
    );
    shm_mem_->initialized();

    coin::Print::info("ctrl mem addr: {:X}, data mem addr: {:X}", (size_t)shm_mem_->ctl_addr(), (size_t)shm_mem_->data_addr());

    auto mem_info = fmt::format("{}.0x{:X}.0x{:X}", node, (size_t)shm_mem_->ctl_addr(), (size_t)shm_mem_->data_addr());
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

    if(not is_memory_block_valid((void*)ctl_addr, ctrl_mem_size))
    {
        coin::Print::error("shm memory is not valid.");
        exit(1);
    }
    if(not is_memory_block_valid((void*)data_addr, data_mem_size))
    {
        coin::Print::error("data memory is not valid.");
        exit(1);
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
