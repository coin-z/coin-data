/**
 * @file shm_manager.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <memory>

#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/file_lock.hpp>
#include <coin-data/local/impl/shm_memory.hpp>

namespace coin::data
{
void set_node_shm_root_path(const std::string& path);

class ShmManager
{
public:
    ShmManager(const std::string& key);
    ShmManager(const std::string& key, const pid_t pid, void* ctl_addr, void* data_addr);
    ~ShmManager();
    ShmManager(const ShmManager&) = delete;
    ShmManager& operator = (const ShmManager&) = delete;

    __inner::ShmMemory& mem();

    static std::string get_root();
    static std::string get_node_root_path(const std::string& node, const std::string& key);
    static std::string get_ctl_key_file  (const std::string& node, const std::string& key);
    static std::string get_data_key_file (const std::string& node, const std::string& key);

    static void update_shm_addr(const std::string& node);

    inline const std::string key_file()  const { return key_file_; }
    inline const std::string data_file() const { return data_file_; }

private:
    const pid_t self_pid_;
    const std::string key_path_;
    const std::string key_file_;
    const std::string data_file_;
    std::unique_ptr<__inner::ShmMemory> shm_mem_;
};
template<typename T>
class FileMapObject
{
    static size_t get_file_size(const std::string& name)
    {
        struct stat st;
        stat(name.c_str(), &st);
        return st.st_size;
    }
public:
    template<typename... Args>
    explicit FileMapObject(const std::string& name, const size_t size, Args&&... args)
      : fd_(-1)
      , obj_(nullptr)
      , name_(name)
      , size_(size)
    {
        fd_ = open(name_.c_str(), O_RDWR | O_CREAT, 0600);
        if(fd_ < 0) coin::Print::error("FileMapObject open file {} failed", name_);
        ftruncate(fd_, size_);
        void* ptr = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        obj_ = new (ptr) T(std::forward<Args>(args)...);
    }
    explicit FileMapObject(const std::string& name)
      : fd_(-1)
      , obj_(nullptr)
      , name_(name)
      , size_(get_file_size(name))
    {
        fd_ = open(name_.c_str(), O_RDWR, 0600);
        if(fd_ < 0) coin::Print::error("FileMapObject open file {} failed", name_);
        void* ptr = mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd_, 0);
        if(ptr == MAP_FAILED)
        {
            perror("mmap");
            coin::Print::error("FileMapObject mmap file {} failed", name_);
        }
        obj_ = static_cast<T*>(ptr);
    }
    ~FileMapObject()
    {
        // unmap the memory
        munmap(obj_, size_);
        if(fd_ >= 0)
        {
            close(fd_);
            fd_ = -1;
        }
    }
    const T* operator -> () const {
        return reinterpret_cast<T*>(obj_);
    }
    T* operator -> () {
        return reinterpret_cast<T*>(obj_);
    }

    inline const std::string name() const { return name_; }
    inline const int fd() const { return fd_; }

    static bool is_valid(const std::string& name)
    {
        return get_file_size(name) > 0;
    }

private:
    int fd_;
    T* obj_;
    const std::string name_;
    const size_t size_;
};
template<typename T>
class FileMapObjectLock
{
public:
    explicit FileMapObjectLock(const T& file_map)
      : file_map_(file_map)
    {
        if(file_map.fd() < 0)
        {
            throw std::runtime_error("FileMapObjectLock file_map fd < 0");
        }
        coin::FileLock::lock(file_map.fd());
    }

    ~FileMapObjectLock()
    {
        if(file_map_.fd() < 0)
        {
            throw std::runtime_error("FileMapObjectLock file_map fd < 0");
        }
        coin::FileLock::unlock(file_map_.fd());
    }

private:
    const T& file_map_;
};
} // namespace coin::data
