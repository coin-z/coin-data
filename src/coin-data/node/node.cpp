#include "node.hpp"
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <fmt/format.h>

#include <coin-commons/utils/utils.hpp>

static const std::string& root_path_() { static const std::string str{"/dev/shm/coin"}; return str; }

static bool is_slink_exist_(const std::string& name)
{
    struct stat statbuf;
    if (lstat(name.c_str(), &statbuf) == 0) {
        if (S_ISLNK(statbuf.st_mode)) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

static bool is_node_exist(const std::string &name)
{
    auto node_path = fmt::format("{}/{}", root_path_(), name);
    auto pid_file = fmt::format("{}/pid", node_path);
    if(not std::filesystem::exists(pid_file))
    {
        return false;
    }

    pid_t p;
    std::ifstream pid_file_in(pid_file);
    pid_file_in >> p;
    pid_file_in.close();
    if(p <= 0 or kill(p, 0) != 0)
    {
        return false;
    }

    auto event_path = fmt::format("{}/{}/events", node_path, p);
    if(not std::filesystem::exists(event_path))
    {
        return false;
    }
    return true;
}
static bool wait_file(const std::string &file)
{
    while(not std::filesystem::exists(file))
    {
        usleep(10);
    }
    return true;
}

static bool wait_pid_file(const std::string& file)
{
    // check file size
    while(true)
    {
        try{
            if(std::filesystem::exists(file) and std::filesystem::file_size(file) > 0)
            { }
            else
            {
                usleep(10); continue;
            }
        }
        catch(const std::exception& e)
        {
            usleep(10); continue;
        }
        std::ifstream pid_file_in(file);
        pid_t p;
        pid_file_in >> p;
        pid_file_in.close();
        if(p <= 0 or kill(p, 0) != 0)
        {
            continue;;
        }
        return true;
    }
    return false;
}
static pid_t read_pid_of_node(const std::string &name)
{
    const auto pid_file_name = fmt::format("{}/{}", root_path_(), name) + "/pid";
    if(not std::filesystem::exists(pid_file_name))
    {
        return -1;
    }
    std::ifstream pid_file(pid_file_name);
    pid_t p;
    pid_file >> p;
    pid_file.close();
    return p;
}
static pid_t wait_and_read_pid_of_node(const std::string &name)
{
    auto pid_file_name = fmt::format("{}/{}", root_path_(), name) + "/pid";
    wait_file(pid_file_name);
    wait_pid_file(pid_file_name);

    return read_pid_of_node(name);
}
namespace coin::node
{
ThisNode::ThisNode(const std::string &name)
  : name_(name)
  , pid_(getpid())
  , node_root_(fmt::format("{}/{}", root_path_(), name))
  , node_path_(fmt::format("{}/{}", node_root_, pid_))
  , event_path_(fmt::format("{}/{}", node_path_, "events"))
  , watch_path_(fmt::format("{}/{}", node_path_, "watch"))
  , observed_path_(fmt::format("{}/{}", node_path_, "observed"))
{
    // check if node is already exist, if exists throw node exists error
    if(is_node_exist(name))
    {
        throw std::runtime_error(fmt::format("node {} already exists", name_));
    }
    else if(std::filesystem::exists(node_root_))
    {
        std::filesystem::remove_all(node_root_);
    }
    // check event path, if not exists, create it
    if(not std::filesystem::exists(event_path_))
    {
        std::filesystem::create_directories(event_path_);
    }
    node_lock_ = (std::make_unique<coin::FileLock>(fmt::format("{}/node.lock", node_root_)));
    coin::FileLockGuard lock(*node_lock_);
    // check watch path, if not exists, create it
    if(not std::filesystem::exists(watch_path_))
    {
        std::filesystem::create_directories(watch_path_);
    }
    // check observed path, if not exists, create it
    if(not std::filesystem::exists(observed_path_))
    {
        std::filesystem::create_directories(observed_path_);
    }

    // write pid to pid file
    std::ofstream pid_file(fmt::format("{}/{}", node_root_, "pid"));
    pid_file << pid_;
    pid_file.close();

    // add default event
    add_event("create");
    add_event("destroy");

    destroy_node_fd_ = open(fmt::format("{}/{}", event_path_, "destroy").c_str(), O_RDONLY, 0600);

    // wait for node monitor to be ready
    usleep(1000);
}
node::ThisNode::~ThisNode()
{
    coin::FileLockGuard lock(*node_lock_);
    notify_destroy_();
}
std::string ThisNode::cvt_event_name_(const std::string& event)
{
    std::string ret = event;
    std::replace(ret.begin(), ret.end(), '/', '.');
    return std::move(ret);
}
bool ThisNode::add_event(const std::string &event)
{
    const std::string e = cvt_event_name_(event);
    // check event already exists
    if(std::filesystem::exists(fmt::format("{}/{}", event_path_, e)))
    {
        return false;
    }
    int fd = open((event_path_ + "/" + e).c_str(), O_RDWR | O_CREAT, 0600);
    if(fd < 0) return false;
    close(fd);
    return true;
}
void ThisNode::notify(const std::string &event)
{
    const std::string e = cvt_event_name_(event);
    int fd = open(fmt::format("{}/{}", event_path_, e).c_str(), O_RDONLY, 0600);
    if(fd > 0)
    {
        close(fd);
    }
    else
    {
        throw std::runtime_error(fmt::format("failed to notify event {}", e));
    }
}
void ThisNode::notify_create()
{
    std::filesystem::directory_iterator directory(node_root_);
    // notify("create");
}
void ThisNode::notify_destroy_()
{
    // remove all resources of this node
    close(destroy_node_fd_);
    std::filesystem::remove_all(node_root_);
}
NodeMonitor::NodeWatcher::NodeWatcher(const std::string &name, std::unique_ptr<callback_map_t>&& callbacks, std::any&& obj)
  : name_(name)
  , pid_(wait_and_read_pid_of_node(name))
  , node_root_(fmt::format("{}/{}", root_path_(), name))
  , node_path_(fmt::format("{}/{}", node_root_, pid_))
  , event_path_(fmt::format("{}/events", node_path_))
  , watch_path_(fmt::format("{}/watch", node_path_))
  , observed_path_(fmt::format("{}/observed", node_path_))
  , callbacks_(std::move(callbacks))
  , object_(obj)
  , node_lock_(std::make_unique<coin::FileLock>(fmt::format("{}/node.lock", node_root_)))
{
}
bool NodeMonitor::NodeWatcher::add_event(const std::string &event, const callback_t &callback)
{
    if(not callbacks_) return false;
    std::string e = event;
    std::replace(e.begin(), e.end(), '/', '.');
    auto itor = callbacks_->find(e);
    if(itor == callbacks_->end())
    {
        callbacks_->emplace(e, callback);
        return true;
    }
    itor->second = callback;
    return true;
}
void NodeMonitor::NodeWatcher::lock() const
{
    node_lock_->lock();
}
void NodeMonitor::NodeWatcher::unlock() const
{
    node_lock_->unlock();
}
void NodeMonitor::NodeWatcher::on_event_notify_(const std::string &event)
{
    auto it = callbacks_->find(event);
    if (it != callbacks_->end())
    {
        it->second(name_, event, object_);
    }
    else
    {
        coin::Print::warn("no event find in callback: {} {}", name_, event);
    }
}
NodeMonitor::NodeMonitor()
  : initializer_()
  , inotify_fd_(inotify_init())
  , root_path_inotify_fd_(inotify_add_watch(inotify_fd_, root_path_().c_str(), IN_CREATE | IN_OPEN))
{
    work_thread_ = std::make_unique<std::thread>(std::bind(&NodeMonitor::work_task_, this));
}
NodeMonitor &NodeMonitor::monitor()
{
    static NodeMonitor mon;
    return mon;
}
NodeMonitor::~NodeMonitor()
{
    is_async_working_ = false;
    work_thread_->join();
    work_thread_.reset();
    close(inotify_fd_);
    inotify_map_.clear();
    inotify_map_rev_.clear();
    inotify_fd_ = -1;
}
std::string NodeMonitor::root_path()
{
    return root_path_();
}
std::string NodeMonitor::node_path(const std::string& name)
{
    pid_t pid = wait_and_read_pid_of_node(name);
    return fmt::format("{}/{}/{}", root_path_(), name, pid);
}

pid_t NodeMonitor::node_pid(const std::string &name)
{
    return read_pid_of_node(name);
}
void NodeMonitor::work_task_()
{
    do {
        // std::this_thread::sleep_for(std::chrono::microseconds(1));
        fd_set rd;
        struct timeval tv;
        int err;
        FD_ZERO(&rd);
        FD_SET(inotify_fd_, &rd);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        err = select(inotify_fd_ + 1, &rd, NULL, NULL, &tv);
        if(err < 0) { perror("select inotify"); }
        else if(err == 0) { continue; }

        // read event
        char buffer[1024];
        ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));
        if (len < 0) { perror("read inotify"); }

        // process event
        for (int i = 0; i < len; ) {
            struct inotify_event *event = reinterpret_cast<struct inotify_event *>(buffer + i);
            i += sizeof(struct inotify_event) + event->len;

            coin::Print::debug("inotify event: {} {}", event->mask, event->name);

            if((event->wd == root_path_inotify_fd_) && (event->mask & (IN_CREATE | IN_OPEN | IN_ISDIR)))
            {
                if(strlen( event->name ) > 0)
                {
                    std::lock_guard<std::mutex> lock(node_exchange_mutex_);
                    auto callback_itor = wait_node_callback_map_.find(event->name);
                    if(callback_itor != wait_node_callback_map_.end())
                    {
                        if(this->add_(event->name, std::move(callback_itor->second)))
                        {
                            wait_node_callback_map_.erase(callback_itor);
                        }
                        else
                        {
                            throw std::runtime_error(fmt::format("add node <{}> failed", event->name));
                        }
                    }
                    else
                    {
                        fmt::print("node<{}> is not exist in monitor map\n", event->name);
                    }
                }
                continue;
            }
            if(event->mask & IN_ISDIR) { continue; }
            if(event->wd != root_path_inotify_fd_ && event->mask & IN_CLOSE_NOWRITE)
            {
                std::lock_guard<std::mutex> lock(node_exchange_mutex_);
                auto node_itor = inotify_map_rev_.find(event->wd);
                if(node_itor != inotify_map_rev_.end())
                {
                    node_itor->second->on_event_notify_(event->name);
                    if(std::string(event->name) == "destroy")
                    {
                        // move node to wait_node_callback_map_
                        wait_node_callback_map_.emplace(node_itor->second->name(), std::move(node_itor->second->callbacks_));
                        remove_(node_itor->second->name());
                    }
                }
            }
        }
    } while (is_async_working_);
    coin::Print::warn("NodeMonitor is work task done.");
}

bool NodeMonitor::notify_event(const std::string &node, const std::string &event)
{
    auto event_file = fmt::format("{}/{}/{}/events/{}",
        root_path(), node, read_pid_of_node(node), ThisNode::cvt_event_name_(event));
    if(not std::filesystem::exists(event_file))
    {
        return false;
    }
    int fd = open(event_file.c_str(), O_RDONLY, 0600);
    if(fd > 0)
    {
        close(fd);
    }
    else
    {
        return false;
    }
    return true;
}

std::unique_ptr<NodeMonitor::NodeWatcher>& NodeMonitor::node_(const std::string &name)
{
    auto wd = inotify_map_.find(name);
    if(wd == inotify_map_.end())
    {
        return null_node_ptr_;
    }
    auto nt = inotify_map_rev_.find(wd->second);
    if(nt == inotify_map_rev_.end())
    {
        return null_node_ptr_;
    }
    return nt->second;
}

bool NodeMonitor::add(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks, std::any&& obj)
{
    std::lock_guard<std::mutex> lock(node_exchange_mutex_);
    const std::string& n = name;
    // check node already monitored, if yes, return false
    if(find(n) > -1)
    {
        return false;
    }
    if(wait_node_callback_map_.find(n) != wait_node_callback_map_.end())
    {
        return false;
    }
    // check node is exist, if not add root path to inotify
    if(not is_node_exist(n))
    {
        wait_node_callback_map_.emplace(n, std::move(callbacks));
        coin::Print::warn("node<{}> is not exist, add root path to inotify", n);
        coin::Print::info("wait node inotify list: {} {}", name, wait_node_callback_map_.size());
        for(auto& item : wait_node_callback_map_)
        {
            coin::Print::info(">> {}", item.first);
        }
        return true;
    }
    return add_(n, std::move(callbacks), std::move(obj));
}
bool NodeMonitor::add_event_to(const std::string &name, const std::string &event, const callback_t &callback)
{
    std::lock_guard<std::mutex> lock(node_exchange_mutex_);
    auto wait_node = wait_node_callback_map_.find(name);
    if(wait_node != wait_node_callback_map_.end())
    {
        wait_node->second->emplace(event, callback);
        return true;
    }

    auto& node = node_(name);
    if(node)
    {
        return node->add_event(event, callback);
    }
    else
    {
        wait_node_callback_map_.emplace(name, make_callback_map({{event, callback}}));
        return true;
    }
    return false;
}
bool NodeMonitor::add_(const std::string &name, std::unique_ptr<NodeWatcher::callback_map_t> &&callbacks, std::any &&obj)
{
    auto node = std::make_unique<NodeWatcher>(name, std::move(callbacks), std::move(obj));

    coin::FileLockGuard lock(node->node_lock());

    auto file = node->event_path();

    // wait_file(file);

    auto itor = inotify_map_.find(name);
    if(itor != inotify_map_.end())
    {
        return false;
    }
    const uint32_t mask = IN_CLOSE;
    auto wd = inotify_add_watch(inotify_fd_, file.c_str(), mask);
    if(wd < 0)
    {
        return false;
    }

    int ret = 0;
    // create watch to this node
    {
        auto from = node->node_path();
        auto to = fmt::format("{}/{}", this_node_->watch_path(), name);
        if(is_slink_exist_(to))
        {
            coin::Print::warn("unlink: {}", to);
            unlink(to.c_str());
        }
        ret = symlink(from.c_str(), to.c_str());
        if(ret != 0) coin::Print::error("symlink failed: {}, from: {}, to: {}", ret, from, to);
    }
    // create observed to node <name>
    {
        auto from = this_node_->node_path();
        auto to = fmt::format("{}/{}", node->observed_path(), this_node_->name());
        if(is_slink_exist_(to))
        {
            coin::Print::warn("unlink: {}", to);
            unlink(to.c_str());
        }
        ret = symlink(from.c_str(), to.c_str());
        if(ret != 0) coin::Print::error("symlink failed: {}, from: {}, to: {}", ret, from, to);
    }

    inotify_map_.emplace(name, wd);
    inotify_map_rev_.emplace(wd, std::move(node));

    coin::Print::info("node inotify list: {} {}", name, inotify_map_.size());
    for(auto& item : inotify_map_)
    {
        coin::Print::info(">> {}", item.first);
    }
    return true;
}
bool NodeMonitor::remove(const std::string &name)
{
    const std::string& n = name;
    remove_(n);
    wait_node_callback_map_.erase(n);
    // remove node <name> from watch
    unlink(this_node_->watch_path().c_str());
    return true;
}
bool NodeMonitor::remove_(const std::string &name)
{
    auto itor = inotify_map_.find(name);
    if(itor == inotify_map_.end())
    {
        fmt::print("remove node <{}> failed, not exist\n", name);
        return false;
    }
    auto wd = itor->second;
    inotify_rm_watch(inotify_fd_, wd);
    inotify_map_.erase(itor);

    auto node_itor = inotify_map_rev_.find(wd);
    if(node_itor == inotify_map_rev_.end())
    {
        fmt::print("remove node <{}> failed, not exist\n", name);
        return false;
    }
    auto& node = node_itor->second;

    // remove node <name> from observed
    unlink(node->observed_path().c_str());

    inotify_map_rev_.erase(node_itor);
    return true;
}
bool NodeMonitor::remove(const int id)
{
    auto itor = inotify_map_rev_.find(id);
    if(itor == inotify_map_rev_.end())
    {
        return false;
    }
    auto& node = itor->second;
    inotify_rm_watch(inotify_fd_, id);
    inotify_map_.erase(node->name());
    inotify_map_rev_.erase(id);
    wait_node_callback_map_.erase(node->name());
    return true;
}

bool NodeMonitor::is_alive(const std::string &name)
{
    if(wait_node_callback_map_.find(name) != wait_node_callback_map_.end())
    {
        return false;
    }
    return true;
}

std::string NodeMonitor::find(const int& id)
{
    auto itor = inotify_map_rev_.find(id);
    if(itor != inotify_map_rev_.end())
    {
        return itor->second->name();
    }
    return std::string();
}
int NodeMonitor::find(const std::string& name)
{
    // replace '/' with '.'
    std::string n = name;
    std::replace(n.begin(), n.end(), '/', '.');
    auto itor = inotify_map_.find(n);
    if(itor != inotify_map_.end())
    {
        return itor->second;
    }
    return -1;
}

std::unique_ptr<NodeMonitor::callback_map_t> NodeMonitor::make_callback_map(std::initializer_list<std::pair<std::string, callback_t>> list)
{
    std::unique_ptr<callback_map_t> map = std::make_unique<callback_map_t>();
    for(auto& item : list)
    {
        map->emplace(item.first, item.second);
    }
    return ( map );
}

NodeMonitor::Initializer::Initializer()
{
    if(not std::filesystem::exists(root_path()))
    {
        std::filesystem::create_directories(root_path());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace coin::node
