#pragma once
#include <map>
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>

namespace coin::node
{
class ThisNode
{
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

private:
    const std::string name_;
    const pid_t pid_;
    const std::string node_root_;
    const std::string node_path_;
    const std::string event_path_;

    int destroy_node_fd_;

    void notify_destroy_();
};

class NodeMonitor
{
private:
    class NodeWatcher
    {
        friend class NodeMonitor;
    public:
        using callback_t = std::function<void(const std::string&, const std::string&)>;
        using callback_map_t = std::map<std::string, callback_t>;
    public:
        NodeWatcher(const std::string& name, std::unique_ptr<callback_map_t>&& callbacks);
        ~NodeWatcher() = default;

        inline const std::string& name() const { return name_; }
        inline const pid_t& pid() const { return pid_; }
        inline const std::string& event_path() const { return event_path_; }

    private:
        const std::string name_;
        const pid_t pid_;
        const std::string event_path_;

        std::unique_ptr<callback_map_t> callbacks_;

        void on_event_notify_(const std::string& event);
    };
public:
    using callback_t = NodeWatcher::callback_t;
    using callback_map_t = NodeWatcher::callback_map_t;
    NodeMonitor(const NodeMonitor&) = delete;
    NodeMonitor& operator=(const NodeMonitor&) = delete;

    static NodeMonitor& monitor();
    ~NodeMonitor();

    bool add(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks = nullptr);
    bool remove(const std::string& name);
    bool remove(const int id);

    std::string find(const int& id);
    int find(const std::string& name);

    static std::unique_ptr<callback_map_t> make_callback_map(std::initializer_list<std::pair<std::string, callback_t>> list);

private:
    NodeMonitor();

    int inotify_fd_;
    const int root_path_inotify_fd_;
    std::map<std::string, int> inotify_map_;
    std::map<int, std::unique_ptr<NodeWatcher>> inotify_map_rev_;

    std::map<std::string, std::unique_ptr<NodeWatcher::callback_map_t>> wait_node_callback_map_;

    bool is_working_ = true;
    std::unique_ptr<std::thread> work_thread_;

    bool add_(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks);
    bool remove_(const std::string& name);
    void work_task_();
};
} // namespace node
