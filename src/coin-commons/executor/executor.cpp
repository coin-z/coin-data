/**
 * @file executor.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <iostream>
#include <fstream>
#include <filesystem>

#include <pty.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>

#include <coin-commons/utils/strings.hpp>

#include "executor.hpp"

namespace coin
{

EnvironmentVariable::EnvironmentVariable() : env_(new char*[max_env_size_])
{
    // initilaize env
    for (int i = 0; i < max_env_size_; i++)
    {
        env_[i] = nullptr;
    }

    // load system environment
    char ** envir = environ;
    int cnt = 0;
    while(*envir)
    {
        int len = strlen(*envir) + 1;
        env_[cnt] = new char[len];
        memcpy(env_[cnt], *envir, len - 1);
        env_[cnt][len - 1] = '\0';
        cnt++;
        envir++;
    }
}
EnvironmentVariable::~EnvironmentVariable()
{
    char **env = env_;
    while(*env)
    {
        delete[] *env;
        env++;
    }
    delete [] env_;
}

int EnvironmentVariable::add_env(const std::string &key, const std::string &value)
{
    auto var = key + "=" + value;
    int len = var.size() + 1;
    char **env = env_;
    while(*env)
    {
        auto item = std::string(*env);
        auto pos = item.find('=');
        if (pos != std::string::npos)
        {
            auto key_item = item.substr(0, pos);
            if(key_item == key)
            {
                delete [] *env;
                *env = new char[len];
                memcpy(*env, var.c_str(), len - 1);
                (*env)[len - 1] = '\0';
                return 0;
            }
        }
        env++;
    }

    *env = new char[len];
    memcpy(*env, var.c_str(), len - 1);
    (*env)[len - 1] = '\0';
 
    return 0;
}
int EnvironmentVariable::set_env(const std::string &key, const std::string &value)
{
    auto var = key + "=" + value;
    int len = var.size() + 2;
    char **env = env_;
    while(*env)
    {
        auto item = std::string(*env);
        auto pos = item.find('=');
        if (pos != std::string::npos)
        {
            auto key_item = item.substr(0, pos);
            if(key_item == key)
            {
                delete [] *env;
                *env = new char[len];
                memcpy(*env, var.c_str(), len - 1);
                (*env)[len] = '\0';
                return 0;
            }
        }
        env++;
    }
    return -1;
}
const std::string EnvironmentVariable::get_env(const std::string &key) const
{
    std::string ret;
    char **env = env_;
    while(*env)
    {
        auto item = std::string(*env);
        auto pos = item.find('=');
        if (pos != std::string::npos)
        {
            auto key_item = item.substr(0, pos);
            if(key_item == key)
            {
                ret = item.substr(pos + 1);
                break;
            }
        }
        env++;
    }
    return ret;
}

Executor::Executor()
  : master_fd_(0)
  , child_pid_(0)
  , isrunning_(true)
  , exit_code_(0)
  , workdir_("")
  , environ_(std::make_shared<EnvironmentVariable>())
{ }

Executor::~Executor()
{
    shutdown();
}
void Executor::change_directory(const std::string& dir)
{
    workdir_ = dir;
}
void Executor::execute(const std::string& cmd, const std::string& args)
{
    execute(cmd, coin::strings::split(args, " "));
}
void Executor::execute(const std::string &cmd, const std::vector<std::string> &args)
{
    if(cmd.empty()) return;

    // check if pty is running
    if(worker_)
    {
        isrunning_ = false;
        worker_->join();
        worker_.reset();
    }

    // check child process id
    if(child_pid_ > 0)
    {
        // kill child process, if it still running
        kill(-child_pid_, SIGKILL);
        child_pid_ = 0;
    }

    // launch pty
    isrunning_ = true;
    int slave = 0;
    int ret = openpty(&master_fd_, &slave, nullptr, nullptr, nullptr);
    if(ret < 0)
    {
        perror("execute openpty");
        exit(-1);
    }
    ret = fork();
    if(ret == 0)
    {
        // close master_fd
        close(master_fd_);
        // launch cmd proc with args
        launch_proc_(slave, cmd, args);
    }
    else
    {
        // record child process id
        child_pid_ = ret;

        // close slave in main proc
        close(slave);

        // start read output from master
        worker_ = std::make_shared<std::thread>(std::bind(&Executor::worker_thread_, this));
    }
}
int Executor::make_cmd_(char** argv, const std::string& cmd)
{
    argv[0] = (char*)malloc(cmd.size() + 1);
    memcpy(argv[0], cmd.c_str(), cmd.size());
    argv[0][cmd.size()] = 0;
    return 1;
}
int Executor::make_argv_(char** argv, const std::string& cmd, const std::vector<std::string>& args)
{
    // load args
    int cnt = 0;
    for(auto itor = args.begin(); itor != args.end(); itor++)
    {
        if(itor->empty())
        {
            argv[cnt] = (char*)malloc(1);
            argv[cnt][0] = '\0';
        }
        else
        {
            argv[cnt] = (char*)malloc(itor->size() + 1);
            memcpy(argv[cnt], itor->c_str(), itor->size());
            argv[cnt][itor->size()] = '\0';                
        }
        cnt++;
    }
    return cnt;
}
void Executor::free_argv_(char** argv)
{
    while(*argv)
    {
        free(*argv);
        argv++;
    }
}
void Executor::launch_proc_(int& slave, const std::string& cmd, const std::vector<std::string>& args)
{
    // set slave as stdin, stdout, stderr
    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);

    // setsid, this proc become new process group leader
    setsid();

    // generate command and args
    char *argv[1024];
    int cnt = 0;

    // make cmd and argv
    cnt = make_cmd_(&argv[0], cmd);
    cnt += make_argv_(&argv[1], cmd, args);

    // change work directory
    if(not workdir_.empty())
    { chdir(workdir_.c_str()); }

    // start up
    argv[cnt] = nullptr;
    int ret = execvpe(cmd.c_str(), argv, environ_->env());
    
    // free argv
    free_argv_(argv);
    
    // log out error info
    if(ret < 0)
    {
        fprintf(stderr, "exec start up failed (%d).\n", ret);
        exit(-1);
    }
}
void Executor::worker_thread_()
{
    while(isrunning_)
    {
        // read output from master fd
        read_master_();
    }
}
void Executor::send_signal(int signal)
{
    if(child_pid_ > 0)
    {
        kill(-child_pid_, signal);
    }
}
void Executor::shutdown(const int timeout)
{
    // terminate child process
    if(child_pid_ > 0)
    {
        int ret = kill(-child_pid_, SIGINT);
    }

    std::shared_ptr<std::thread> monitor_thread;
    if(timeout > 0)
    {
        // wait child process in a new thread
        monitor_thread = std::make_shared<std::thread>([this, timeout]
        {
            auto start = std::chrono::steady_clock::now();
            while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeout)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if(!isrunning_)
                { break; }
            }
            // is child proc still running, kill it by SIGKILL
            if(isrunning_)
            {
                send_signal(SIGKILL);
            }
        });
    }
    // wait for exist
    int ret = wait();
    if(exit_notification_)
    { exit_notification_(ret); }

    // clear flags
    isrunning_ = false;
    child_pid_ = 0;

    if(worker_)
    {
        worker_->join();
        worker_.reset();
    }

    if(monitor_thread)
    {
        monitor_thread->join();
        monitor_thread.reset();
    }

    if(master_fd_ != 0)
    {
        read_master_();
        close(master_fd_);
        master_fd_ = 0;
    }
}
void Executor::force_shutdown()
{
    // shutdown in 100ms
    shutdown(100);
}
std::shared_ptr<EnvironmentVariable> Executor::env()
{
    return environ_;
}
void Executor::send(const std::string &input)
{
    write(master_fd_, input.c_str(), input.size());
}
void Executor::set_reveive_callback(const Callback &callback) noexcept
{
    callback_ = callback;
}
void Executor::set_receive_notification(const Notification &callback) noexcept
{
    notification_ = callback;
}
void Executor::set_exit_notification(const ExitNotification &callback) noexcept
{
    exit_notification_ = callback;
}
const bool Executor::has_callback() const noexcept
{
    return callback_ != nullptr;
}
const std::string Executor::read_front()
{
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if(buffer_.size() > 0)
    {
        std::string ret = buffer_.front();
        buffer_.pop_front();
        return std::move(ret);
    }
    return std::string();
}
int Executor::wait()
{
    if(child_pid_ == 0)
    {
        return exit_code_;
    }
    waitpid(child_pid_, &exit_code_, 0);
    return exit_code_;
}
void Executor::read_master_()
{
    // set read fds set
    fd_set read_fds;

    // set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000000; // 1s

    // monitoring master_fd is readable by select
    FD_ZERO(&read_fds);
    FD_SET(master_fd_, &read_fds);
    int result = select(master_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if ( result == 0 )
    { return; }
    else if(result < 0) { /* Nothing to do */ }

    // read data and process
    if (FD_ISSET(master_fd_, &read_fds))
    {
        char buf[4096];
        ssize_t read_size = read(master_fd_, buf, sizeof(buf));
        if (read_size > 0)
        {
            if(callback_)
            {
                callback_(std::string(buf, read_size));
            }
            else if(notification_)
            {
                // if registe receive notification, save data to buffer
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex_);
                    buffer_.push_back(std::string(buf, read_size));
                }
                // invoke receive notification
                {
                    notification_();
                }
            }
            else
            {
                // no callback or notification, print std out
                {
                    std::cout << std::string(buf, read_size);
                }
            }
        }
        else if (read_size == 0)
        {
            // master fd closed, can exit loop
            return;
        }
        else
        {
            if(exit_notification_) { exit_notification_(exit_code_); }
            isrunning_ = false;
            if(master_fd_ != 0)
            {
                close(master_fd_);
                master_fd_ = 0;
            }
            return;
        }
    }
}
} // namespace coin
