#include "node.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <fmt/format.h>
#include <signal.h>

static const std::string root_path = "/dev/shm/coin";

static bool is_node_exist(const std::string &name)
{
    auto node_path = fmt::format("{}/{}", root_path, name);
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
        if(std::filesystem::file_size(file) == 0)
        {
            continue;
        }
        {
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
    }
}

namespace coin::node
{
ThisNode::ThisNode(const std::string &name)
  : name_(name)
  , pid_(getpid())
  , node_root_(fmt::format("{}/{}", root_path, name))
  , node_path_(fmt::format("{}/{}", node_root_, pid_))
  , event_path_(fmt::format("{}/{}", node_path_, "events"))
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
    std::cout << "release exit node fd: " << destroy_node_fd_ << std::endl;
    notify_destroy_();
}
bool ThisNode::add_event(const std::string &event)
{
    // check event already exists
    if(std::filesystem::exists(fmt::format("{}/{}", event_path_, event)))
    {
        return false;
    }
    int fd = open((event_path_ + "/" + event).c_str(), O_RDWR | O_CREAT, 0600);
    if(fd < 0) return false;
    close(fd);
    return true;
}

void ThisNode::notify(const std::string &event)
{
    int fd = open(fmt::format("{}/{}", event_path_, event).c_str(), O_RDONLY, 0600);
    if(fd > 0)
    {
        close(fd);
    }
}
void ThisNode::notify_create()
{
    notify("create");
}
void ThisNode::notify_destroy_()
{
    // remove all resources of this node
    close(destroy_node_fd_);
    std::filesystem::remove_all(node_root_);
}
static pid_t read_pid_of_node(const std::string &name)
{
    auto pid_file_name = fmt::format("{}/{}", root_path, name) + "/pid";
    wait_file(pid_file_name);
    wait_pid_file(pid_file_name);
    std::ifstream pid_file(pid_file_name);
    pid_t p;
    pid_file >> p;
    pid_file.close();
    return p;
}
NodeMonitor::NodeWatcher::NodeWatcher(const std::string &name, std::unique_ptr<callback_map_t>&& callbacks)
  : name_(name)
  , pid_(read_pid_of_node(name))
  , event_path_(fmt::format("{}/{}/{}/events", root_path, name, pid_))
  , callbacks_(std::move(callbacks))
{
}

void NodeMonitor::NodeWatcher::on_event_notify_(const std::string &event)
{
    auto it = callbacks_->find(event);
    if(it != callbacks_->end())
    {
        it->second(name_, event);
    }
}

NodeMonitor::NodeMonitor()
  : inotify_fd_(inotify_init())
  , root_path_inotify_fd_(inotify_add_watch(inotify_fd_, root_path.c_str(), IN_CREATE))
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
    usleep(1000);
    is_working_ = false;
    work_thread_->join();
    work_thread_.reset();
    close(inotify_fd_);
    inotify_map_.clear();
    inotify_map_rev_.clear();
    inotify_fd_ = -1;
}
void NodeMonitor::work_task_()
{
    do {
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

            if(event->mask & IN_CREATE)
            {
                if(strlen( event->name ) > 0)
                {
                    bool r = false;
                    auto callback_itor = wait_node_callback_map_.find(event->name);
                    if(callback_itor != wait_node_callback_map_.end())
                    {
                        r = this->add_(event->name, std::move(callback_itor->second));
                        wait_node_callback_map_.erase(callback_itor);
                    }
                    else
                    {
                        std::cout << "node: " << event->name << " not exist in wait node map." << std::endl;
                    }
                    if(not r)
                    {
                        throw std::runtime_error(fmt::format("add node <{}> failed", event->name));
                    }
                }
            }
            if(event->mask & IN_ISDIR) { continue; }
            if(event->mask & IN_CLOSE_NOWRITE)
            {
                auto node_itor = inotify_map_rev_.find(event->wd);
                if(node_itor != inotify_map_rev_.end())
                {
                    node_itor->second->on_event_notify_(event->name);
                    if(std::string(event->name) == "destroy")
                    {
                        {
                            // move node to wait_node_callback_map_
                            wait_node_callback_map_.emplace(node_itor->second->name(), std::move(node_itor->second->callbacks_));
                        }
                        remove_(node_itor->second->name());
                    }
                }

            }
        }
    } while (is_working_);
}

bool NodeMonitor::add(const std::string& name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks)
{
    // check node already monitored, if yes, return false
    if(find(name) > -1)
    {
        return false;
    }
    if(wait_node_callback_map_.find(name) != wait_node_callback_map_.end())
    {
        return false;
    }
    // check node is exist, if not add root path to inotify
    if(not is_node_exist(name))
    {
        wait_node_callback_map_.emplace(name, std::move(callbacks));
        return true;
    }
    return add_(name, std::move(callbacks));
}
bool NodeMonitor::add_(const std::string &name, std::unique_ptr<NodeWatcher::callback_map_t>&& callbacks)
{
    auto node = std::make_unique<NodeWatcher>(name, std::move(callbacks));
    auto file = node->event_path();

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
    inotify_map_.emplace(name, wd);
    inotify_map_rev_.emplace(wd, std::move(node));
    return true;
}
bool NodeMonitor::remove(const std::string &name)
{
    remove_(name);
    wait_node_callback_map_.erase(name);
    return true;
}
bool NodeMonitor::remove_(const std::string &name)
{
    auto itor = inotify_map_.find(name);
    if(itor == inotify_map_.end())
    {
        std::cout << "remove node <" << name << "> failed, not exist" << std::endl;
        return false;
    }
    auto wd = itor->second;
    inotify_rm_watch(inotify_fd_, wd);
    inotify_map_.erase(itor);
    inotify_map_rev_.erase(wd);
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
    auto itor = inotify_map_.find(name);
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

} // namespace node
