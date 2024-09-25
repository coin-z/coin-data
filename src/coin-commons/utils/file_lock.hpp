/**
 * @file file_lock.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once
#include <string>

namespace coin
{
class FileLock
{
public:
    FileLock(const std::string& path) noexcept;
    ~FileLock() noexcept;

    void lock() noexcept;
    void unlock() noexcept;

    static void lock(const int fd);
    static void unlock(const int fd);

private:
    std::string file_path_;
    int file_descriptor_;
};

class FileLockGuard
{
public:
    FileLockGuard(FileLock& lock) noexcept;
    ~FileLockGuard() noexcept;

private:
    FileLock& lock_;
};
} // namespace coin
