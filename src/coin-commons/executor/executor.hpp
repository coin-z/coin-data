/**
 * @file executor.hpp
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
#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <vector>
#include <functional>

namespace coin
{

class EnvironmentVariable
{

public:
    EnvironmentVariable();
    ~EnvironmentVariable();

    int add_env(const std::string &key, const std::string &value);
    int set_env(const std::string &key, const std::string &value);
    const std::string get_env(const std::string &key) const;
    inline char** env() const { return env_; }

private:
    const int max_env_size_ = 512;
    char ** env_;
};

/**
 * @brief Executor
 * Launch a process in a new environment(pty)
 */
class Executor
{
public:
    using Callback = std::function<void(const std::string&)>;
    using Notification = std::function<void()>;
    using ExitNotification = std::function<void(int)>;

    Executor();
    ~Executor();

    /**
     * @brief change work directory
     * @param dir target directory
     */
    void change_directory(const std::string& dir);
    
    /**
     * @brief execute command
     * @param cmd command
     * @param args arguments
     */
    void execute(const std::string& cmd, const std::string& args);
    void execute(const std::string& cmd, const std::vector<std::string>& args = {});

    /**
     * @brief shutdown the proc launched by executor
     * @param timeout timeout in ms
     * -1 means wait forever
     * force_shutdown will shutdown in 100ms
     */
    void shutdown(const int timeout = -1 /* ms */);
    void force_shutdown();

    /**
     * @brief get environment of this executor
     */
    std::shared_ptr<EnvironmentVariable> env();

    /**
     * @brief send a command line to executor
     * @param input command line of executor
     * @note end with '\n'
     */
    void send(const std::string& input);

    /**
     * @brief send a signal to the proc of this executor
     * @param signal signal to send
     */
    void send_signal(int signal);
    /**
     * @brief wait for the proc exit
     * @return exit code
     */
    int wait();

    /**
     * @brief read front buffer of proc output
     */
    const std::string read_front();

    /**
     * @brief get exit code
     */
    inline int exist_code() const noexcept { return exit_code_; }

    void set_reveive_callback(const Callback& callback) noexcept;
    void set_receive_notification(const Notification& callback) noexcept;
    void set_exit_notification(const ExitNotification& callback) noexcept;
    const bool has_callback() const noexcept;

private:
    int  master_fd_;
    int  child_pid_;
    bool isrunning_;
    int  exit_code_;

    std::string workdir_;

    std::shared_ptr<EnvironmentVariable> environ_;

    mutable std::mutex buffer_mutex_;
    std::deque<std::string> buffer_;
    
    Callback callback_;
    Notification notification_;
    ExitNotification exit_notification_;
    
    std::shared_ptr<std::thread> worker_;

    void read_master_();
    void worker_thread_();
    int make_cmd_(char** argv, const std::string& cmd);
    int make_argv_(char** argv, const std::string& cmd, const std::vector<std::string>& args);
    void free_argv_(char** argv);
    void launch_proc_(int& slave, const std::string& cmd, const std::vector<std::string>& args);
};
} // namespace coin

