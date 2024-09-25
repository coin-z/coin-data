/**
 * @file file_lock.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <unistd.h>
#include <sys/file.h>
#include <stdexcept>

#include "utils/file_lock.hpp"

namespace coin
{
FileLock::FileLock(const std::string &path) noexcept : file_path_(path)
{ }

FileLock::~FileLock() noexcept
{ }

void FileLock::lock() noexcept
{
    // open file
    file_descriptor_ = open(file_path_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (file_descriptor_ == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    lock(file_descriptor_);
}
void FileLock::unlock() noexcept
{
    unlock(file_descriptor_);

    // close file
    close(file_descriptor_);
    if(0 == access(file_path_.c_str(), F_OK))
    {
        // remove file, if it exists
        if (remove(file_path_.c_str()) != 0) {
            throw std::runtime_error("Error releasing file lock");
        }
    }
}

void FileLock::lock(const int fd)
{
    // get lock
    struct flock lock;
    lock.l_type = F_WRLCK;  // write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;         // lock total file
    lock.l_pid = getpid();  // process id

    // attempt to acquire lock, if lock is already held,
    // wait for it to be released
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error acquiring lock");
        close(fd);
        exit(EXIT_FAILURE);
    }
}
void FileLock::unlock(const int fd)
{
    // release file lock
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error releasing lock");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

FileLockGuard::FileLockGuard(FileLock &lock) noexcept : lock_(lock)
{
    lock_.lock();
}
FileLockGuard::~FileLockGuard() noexcept
{
    lock_.unlock();
}
} // namespace coin
