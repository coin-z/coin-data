#pragma once
#include <map>
#include <any>
#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <functional>

#include <coin-commons/utils/file_lock.hpp>

namespace coin::node
{
class NodeMonitor;
class ThisNode
{
friend class NodeMonitor;
public:
    ThisNode(const std::string& name);
    virtual ~ThisNode();

    void notify(const std::string& event);
    bool add_event(const std::string& event);

protected:
    void notify_create();

    inline const std::string& name() const { return name_; }
    inline const pid_t& pid() const { return pid_; }
    inline const std::string& node_root() const { return node_root_; }
    inline const std::string& node_path() const { return node_path_; }
    inline const std::string& event_path() const { return event_path_; }
    inline const std::string& watch_path() const { return watch_path_; }
    inline const std::string& observed_path() const { return observed_path_; }

private:
    const std::string name_;
    const pid_t pid_;
    const std::string node_root_;
    const std::string node_path_;
    const std::string event_path_;
    const std::string watch_path_;
    const std::string observed_path_;

    int destroy_node_fd_;

    std::unique_ptr<coin::FileLock> node_lock_;
    void notify_destroy_();

    static std::string cvt_event_name_(const std::string& event);
};

class NodeMonitor
{
private:
    class NodeWatcher
    {
        friend class NodeMonitor;
    public:
        using callback_t = std::function<void(const std::string&, const std::string&, std::any&)>;
        using callback_map_t = std::map<std::string, callback_t>;
    public:
        NodeWatcher(const std::string& name, std::unique_ptr<callback_map_t>&& callbacks, std::any&& obj = nullptr);
        ~NodeWatcher() = default;

        inline const std::string& name() const { return name_; }
        inline const pid_t& pid() const { return pid_; }
        inline const std::string& node_path() const { return node_path_; }
        inline const std::string& event_path() const { return event_path_; }
        inline const std::string& watch_path() const { return watch_path_; }
        inline const std::string& observed_path() const { return observed_path_; }

        bool add_event(const std::string& event, const callback_t& callback);

        inline coin::FileLock& node_lock() { return *node_lock_; }
        void lock() const;
        void unlock() const;

    private:
        const std::string name_;
        const pid_t pid_;
        const std::string node_root_;
        const std::string node_path_;
        const std::string event_path_;
        const std::string watch_path_;
        const std::string observed_path_;

        std::unique_ptr<callback_map_t> callbacks_;

        std::any object_;

        std::unique_ptr<coin::FileLock> node_lock_;

        void on_event_notify_(const std::string& event);
    };
public:
    using callback_t = NodeWatcher::callback_t;
    using callback_map_t = NodeWatcher::callback_map_t;
    NodeMonitor(const NodeMonitor&) = delete;
    NodeMonitor& operator=(const NodeMonitor&) = delete;

    static NodeMonitor& monitor();
    ~NodeMonitor();

    template<typename NodeType = ThisNode>
    void init(const std::string& name);

    inline std::shared_ptr<ThisNode> this_node() { return this_node_; }

    bool add(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks = std::make_unique<callback_map_t>(), std::any&& obj = nullptr);
    bool add_event_to(const std::string& name, const std::string& event, const callback_t& callback);
    bool remove(const std::string& name);
    bool remove(const int id);

    bool is_alive(const std::string& name);

    std::string find(const int& id);
    int find(const std::string& name);

    static std::string root_path();
    static std::string node_path(const std::string& name);
    static pid_t node_pid(const std::string& name);

    static std::unique_ptr<callback_map_t> make_callback_map(std::initializer_list<std::pair<std::string, callback_t>> list);

    static bool notify_event(const std::string& node, const std::string& event);


private:
    NodeMonitor();
    struct Initializer {
        Initializer();
        ~Initializer() = default;
    };

    const Initializer initializer_;

    std::shared_ptr<ThisNode> this_node_;


    int inotify_fd_;
    const int root_path_inotify_fd_;
    std::map<std::string, int> inotify_map_;
    std::map<int, std::unique_ptr<NodeWatcher>> inotify_map_rev_;

    std::map<std::string, std::unique_ptr<NodeWatcher::callback_map_t>> wait_node_callback_map_;

    std::mutex node_exchange_mutex_;

    bool is_async_working_ = true;
    std::unique_ptr<std::thread> work_thread_;

    bool add_(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks, std::any&& obj = nullptr);
    bool remove_(const std::string& name);
    void work_task_();

    std::unique_ptr<NodeWatcher>& node_(const std::string& name);
    std::unique_ptr<NodeWatcher> null_node_ptr_;
};
template <typename NodeType>
inline void node::NodeMonitor::init(const std::string &name)
{
    this_node_ = \
        std::static_pointer_cast<ThisNode>( std::make_shared<NodeType>(name) );
    this_node_->notify_create();
}
} // namespace node
