/**
 * @file shmobj_manager.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "shmobj_manager.hpp"

#include <sys/inotify.h>
#include <sys/types.h>

#include <tuple>
#include <regex>
#include <mutex>
#include <thread>
#include <filesystem>

#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/crc32.hpp>
#include <coin-data/local/impl/shmobj_manager.hpp>


namespace coin::data
{
ShmObjManager::ShmObjManager()
  : self_pid_(getpid())
  , inotify_monitor_(
        std::make_unique<INotifyMonitor>(
            std::bind(&ShmObjManager::inotify_shared_object_monitor_, this, std::placeholders::_1))
    )
{ }
ShmObjManager::~ShmObjManager()
{
    // check shm memory state
    if(not self_node_->second.shm_manager->mem().is_busy())
    {
        // release object info
        Allocator<ShmMap<ShmString, ObjectInfo>> shm_obj_map_allocator;
        auto& map = *(*self_node_->second.shared_info)->shm_obj_map;
        map.clear();

        shm_obj_map_size_.store(0);
    }
}

std::shared_ptr<ShmObjManager> ShmObjManager::instance_()
{
    // shm obj manager must be singleton, and do not copy, do not move,
    // so we can only create once with operator new.
    static std::shared_ptr<ShmObjManager> shm_obj_manager(new ShmObjManager());
    return shm_obj_manager;
}
ShmObjManager &ShmObjManager::instance()
{
    return *instance_();
}
void ShmObjManager::init(const std::string& node)
{
    node_name_ = node;
    // Step0. clear shm file that not available
    clear_invalid_shm_(node);

    // Step1. create shm manager of this process
    shm_manager_ = std::make_shared<ShmManager>(node);
    auto shm_manager = shm_manager_;

    // Step2. create shm obj map and shm obj map mutex, index object in shm by key
    Allocator<ShmMap<ShmString, ObjectInfo*>> shm_obj_map_allocator;
    auto shm_obj_map_ptr = shm_obj_map_allocator.allocate(1);
    shm_obj_map_allocator.construct(shm_obj_map_ptr);
    auto remove_map_ptr = shm_obj_map_allocator.allocate(1);
    shm_obj_map_allocator.construct(remove_map_ptr);
    Allocator<ProcessMutex> process_mutex_allocator;
    auto mutex_ptr = process_mutex_allocator.allocate(1);
    process_mutex_allocator.construct(mutex_ptr);

    // Step3. create file map obj for shm memroy entry
    shared_info_ = 
        std::make_shared<FileMapObject<ShmSystemSharedInfo>>(
            shm_manager->key_file(),
            __inner::ShmMemory::PAGE_SIZE,
            self_pid_,
            shm_manager->mem().ctl_addr(),
            shm_manager->mem().data_addr(),
            shm_obj_map_ptr, remove_map_ptr, mutex_ptr
        );
    auto shared_info = shared_info_;

    // Step4. add node info to node_map_
    {
        std::lock_guard<std::mutex> lock(node_map_mutex_);
        node_map_.emplace(node_name_, ExtNodeMapItem(std::move(shared_info), std::move(shm_manager)));
        self_node_ = node_map_.find(node_name_);
    }

    // Step5. create thread to monitor node
    // thread to monitor process status
    std::thread(std::bind(&ShmObjManager::node_monitor_, this)).detach();
}
__inner::ShmMemory& ShmObjManager::mem()
{
    return shm_manager_->mem();
}
uint64_t ShmObjManager::getSharedObjMapUpdateTime()
{
    return (*self_node_->second.shared_info)->update_time.load();
}
const std::size_t ShmObjManager::object_map_size() const
{
    return shm_obj_map_size_.load();
}
bool ShmObjManager::hasSharedObject(const std::string &name)
{
    {
        ProcessLockGuard<ProcessMutex> lock(*(*self_node_->second.shared_info)->shm_obj_map_mutex);
        auto it = (*self_node_->second.shared_info)->shm_obj_map->find(from_std_string(name));
        if(it != (*self_node_->second.shared_info)->shm_obj_map->end())
        {
            return true;
        }
    }
    return false;
}
bool ShmObjManager::discovery_ext_map_(const std::string &node, const std::string &key, std::shared_ptr<SharedObjectRetentionBase>& ptr)
{
    // Step1. find node in node_map_
    auto node_itor = node_map_.find(node);
    if(node_itor == node_map_.end())
    {
        return false;
    }

    // Step2. find key in obj_list of node
    auto obj_itor = node_itor->second.obj_list.find(key);
    if(obj_itor != node_itor->second.obj_list.end())
    {
        ptr = std::dynamic_pointer_cast<SharedObjectRetentionBase>(obj_itor->second);
        coin::Print::debug("find in ext map object list: {}", node);
        return true;
    }

    // Step3. find key in obj_removed_list of node
    auto removed_obj_itor = node_itor->second.obj_removed_list.find(key);
    if(removed_obj_itor != node_itor->second.obj_removed_list.end())
    {
        ptr = std::dynamic_pointer_cast<SharedObjectRetentionBase>(removed_obj_itor->second);
        coin::Print::debug("find in ext map object removed list: {}", node);
        return true;
    }

    // Not found, return false
    return false;
}
bool ShmObjManager::discovery_shm_(const std::string &node, const std::string &key, std::shared_ptr<SharedObjectRetentionBase>& ptr)
{
    bool ret = false;
    // find wheather node exist
    auto node_path = ShmManager::get_node_root_path(node);
    // check node_path exist
    if(not std::filesystem::exists(node_path))
    {
        coin::Print::debug("node path not exist: {}", node_path);
        return ret;
    }
    coin::Print::debug("lookup node path exist: {}", node_path);
    std::regex node_regex("[0-9]+");
    // itearate all files in node_path
    for(auto& p : std::filesystem::directory_iterator(node_path))
    {
        coin::Print::debug("check file: {}", p.path().filename().string());
        if(not p.is_regular_file() or not std::regex_match(p.path().filename().string(), node_regex))
        { continue; }

        // check node is alive, if not alive, jump to next
        pid_t pid = std::atoi(p.path().filename().string().c_str());
        coin::Print::debug("check process alive pid: {}", pid);
        if(kill(pid, 0) != 0)
        {
            coin::Print::debug("<{}:{}> is not alive, jump it.", node, pid);
            continue;
        }

        // get file name
        auto file_name = p.path().string();
        if(not FileMapObject<ShmSystemSharedInfo>::is_valid(file_name))
        {
            // file of shm is not valid anymore, no one can use it, just remove it here.
            auto ctl = ShmManager::get_ctl_key_file(node, p.path().filename().string());
            auto data = ShmManager::get_data_key_file(node, p.path().filename().string());
            std::filesystem::remove(ctl);
            std::filesystem::remove(data);

            coin::Print::debug("{} is not valid, remove {} and {}", file_name, ctl, data);

            continue;
        }

        coin::Print::debug("discover node file: {}", file_name);
        std::pair<std::shared_ptr<FileMapObject<ShmSystemSharedInfo>>, std::shared_ptr<ShmManager>> shared_shm_manager;

        static auto make_shared_shm_manager_ = [](const std::string& file_name, const std::string& node, const std::string& key)
        -> std::pair<std::shared_ptr<FileMapObject<ShmSystemSharedInfo>>, std::shared_ptr<ShmManager>>
        {
            auto shared_info = std::make_shared<FileMapObject<ShmSystemSharedInfo>>(file_name);
            auto shm_manager = std::make_shared<ShmManager>(node,
                                                            (*shared_info)->self_pid,
                                                            (*shared_info)->shm_mem_ptr,
                                                            (*shared_info)->shm_mem_data_ptr);

            coin::Print::debug("attach node: {}, key: {}, pid: {}", node, key, (*shared_info)->self_pid);
            coin::Print::debug("ptr: {}, data ptr: {}", (*shared_info)->shm_mem_ptr, (*shared_info)->shm_mem_data_ptr);

            return std::move(
                std::make_pair(
                    std::move(shared_info), std::move(shm_manager)
                )
            );
        };
        
        auto node_itor = node_map_.find(node);
        if(node_itor == node_map_.end())
        {
            shared_shm_manager = make_shared_shm_manager_(file_name, node, key);
        }
        else if((not node_itor->second.shared_info) or (not node_itor->second.shm_manager))
        {
            shared_shm_manager = make_shared_shm_manager_(file_name, node, key);

            node_itor->second.shared_info = shared_shm_manager.first;
            node_itor->second.shm_manager = shared_shm_manager.second;
        }
        else
        {
            shared_shm_manager.first = node_itor->second.shared_info;
            shared_shm_manager.second = node_itor->second.shm_manager;
            coin::Print::debug("attach node: {}, key: {}, pid: {}", node, key, (*shared_shm_manager.first)->self_pid);
            coin::Print::debug("ptr: {}, data ptr: {}", (*shared_shm_manager.first)->shm_mem_ptr, (*shared_shm_manager.first)->shm_mem_data_ptr);
        }

        if(not shared_shm_manager.second->mem().is_busy())
        {
            coin::Print::warn("{}:{} is not busy, remove it.", node, (*shared_shm_manager.first)->self_pid);
            shared_shm_manager.first.reset();
            shared_shm_manager.second.reset();
            continue;
        }

        ProcessLockArea<ProcessMutex> shm_obj_map_lock(*(*shared_shm_manager.first)->shm_obj_map_mutex,
        [&ptr, &key, &node, &node_itor, &shared_shm_manager, &ret, this]
        {
            auto itor = (*shared_shm_manager.first)->shm_obj_map->find(ShmString(key));
            if(itor != (*shared_shm_manager.first)->shm_obj_map->end())
            {
                if(node_itor == node_map_.end())
                {
                    // record in ext node map
                    auto node_item = node_map_.emplace(node,
                        ExtNodeMapItem(std::move(shared_shm_manager.first), std::move(shared_shm_manager.second))
                    );
                    node_itor = node_item.first;
                }
                ProcessLockArea<ProcessMutex> obj_lock(itor->second->obj_mutex, [&itor]
                {
                    itor->second->ref_cnt.fetch_add(1);
                });
                if(not ptr)
                {
                    ptr = std::make_shared<SharedObjectRetentionBase>(node, itor->second);
                }
                else
                {
                    ptr->node_name_ = node;
                    ptr->ptr_ = itor->second;
                }
                ptr->shm_manager = node_itor->second.shm_manager;
                ptr->shared_info = node_itor->second.shared_info;
                auto& item = node_itor->second;
                item.obj_list.emplace(key, ptr);
                ret = true;
            }
            else
            {
                coin::Print::debug(
                    "size of shm_obj_map: {}, not find in node filesystem: /{}/{}",
                    (*shared_shm_manager.first)->shm_obj_map->size(), node, key);
            }
        });
        if(ret)
        {
            return ret;
        }
    }

    return false;
}
void ShmObjManager::node_monitor_()
{
    char buffer[1024];
    coin::Print::info("process monitor thread start.");
    while (coin::ok()) {
        {
            std::lock_guard<std::mutex> lock(node_map_mutex_);
            for(auto node_itor = node_map_.begin(); node_itor != node_map_.end(); /*node_itor*/)
            {
                if(not node_itor->second.shm_manager or not node_itor->second.shared_info)
                {
                    coin::Print::debug("node {} is not setup yet.", node_itor->first);
                }
                else if(kill((*node_itor->second.shared_info)->self_pid, 0) != 0)
                {
                    pid_t node_pid = (*node_itor->second.shared_info)->self_pid;
                    coin::Print::debug("process {} is not alive.", node_pid);
                    auto& obj_list = node_itor->second.obj_list;
                    for(auto obj_itor = obj_list.begin(); obj_itor != obj_list.end(); /*obj_itor*/)
                    {
                        auto release_shared_obj_ = [&obj_itor, &node_itor, &obj_list]{
                            obj_itor->second->shared_info.reset();
                            obj_itor->second->shm_manager.reset();
                            obj_itor->second->ptr_->ref_cnt.fetch_sub(1);
                            obj_itor->second->ptr_->is_destroy = true;
                        };
                        ProcessLockArea<ProcessMutex> obj_lock(obj_itor->second->ptr_->obj_mutex,
                            release_shared_obj_, release_shared_obj_);

                        node_itor->second.obj_removed_list.emplace(obj_itor->first, obj_itor->second);
                        obj_itor = obj_list.erase(obj_itor);
                    }

                    node_itor->second.shared_info.reset();
                    node_itor->second.shm_manager.reset();
                }
                else
                {
                }
                ++node_itor;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    coin::Print::warn("process monitor thread exit.");
}
void ShmObjManager::inotify_shared_object_monitor_(const inotify_event &event)
{
    coin::Print::debug("inotify event: {:X} - {}", event.mask, event.name);

    if(event.mask & IN_ISDIR)
    {
        coin::Print::debug("inotify event: {} is dir", event.name);
        return;
    }

    std::regex node_regex("[0-9]+(.mem.data){0,1}");
    if(std::regex_match(event.name, node_regex)) if(((event.mask & IN_CLOSE)))
    {
        auto notify_file = inotify_monitor_->find(event.wd);
        auto node_name = notify_file.substr(notify_file.find_last_of('/') + 1);
        coin::Print::debug("event notify: {}({}), node name: {}", notify_file, event.wd, node_name);

        {
            std::lock_guard<std::mutex> node_map_lock(node_map_mutex_);
            auto node_itor = node_map_.find(node_name);
            if(node_itor != node_map_.end())
            {
                auto& obj_removed_list = node_itor->second.obj_removed_list;
                for(auto obj_itor = obj_removed_list.begin(); obj_itor != obj_removed_list.end();)
                {
                    coin::Print::debug("try to find <{}> in node <{}>", obj_itor->first, node_name);
                    if( discovery_shm_(node_name, obj_itor->first, obj_itor->second) )
                    {
                        coin::Print::debug("find object <{}> in node <{}>, remove it from removed map.", obj_itor->first, node_name);
                        obj_itor = obj_removed_list.erase(obj_itor);
                    }
                    else
                    {
                        ++obj_itor;
                    }
                }

                auto& obj_list = node_itor->second.obj_list;
                for(auto obj_itor = obj_list.begin(); obj_itor != obj_list.end();)
                {
                    if(obj_itor->second->ptr_->is_destroy)
                    {
                        ProcessLockArea<ProcessMutex> obj_lock(obj_itor->second->ptr_->obj_mutex,
                        [&obj_itor, &node_itor] {
                            obj_itor->second->ptr_->ref_cnt.fetch_sub(1);
                        });
                        node_itor->second.obj_removed_list.emplace(obj_itor->first, obj_itor->second);
                        obj_itor = obj_list.erase(obj_itor);
                    }
                    else
                    {
                        ++obj_itor;
                    }
                }
            }
            else
            {
                coin::Print::debug("node <{}> not found in node map.", node_name);
            }
        }
    }
}
void ShmObjManager::clear_shm_obj_remove_map_(ExtNodeMapItem& node)
{
    auto& obj_removed_map = (*node.shared_info)->shm_obj_removed_map;
    // check removed map item, if not reference by outside, release it.
    for(auto item = obj_removed_map->begin(); item != obj_removed_map->end();)
    {
        if(item->second->ref_cnt.load() == 0)
        {
            coin::Print::debug("release object <{}> reference count: {}, is destroy: {}",
                item->first, item->second->ref_cnt.load(), item->second->is_destroy);

            if(item->second->created_pid != getpid())
            {
                coin::Print::error("release object created by other process: {}", item->first);
                throw std::runtime_error("release object created by other process");
            }
            if(item->second->release_func_addr == 0)
            {
                coin::Print::error("release object has no release function: {}", item->first);
                throw std::runtime_error("release object has no release function");
            }

            auto release_func = 
                reinterpret_cast<std::function<void(void*)>*>((void*)item->second->release_func_addr);
            (*release_func)(item->second->obj_ptr.ptr);
            ObjectInfo::destroy(item->second);
            item = obj_removed_map->erase(item);
        }
        else
        {
            ++item;
        }
    }
    coin::Print::debug("size of obj removed map after clear: {}", obj_removed_map->size());
}
std::pair<std::string, std::string> ShmObjManager::read_node_idx_(const std::string &str)
{
    std::pair<std::string, std::string> ret;
    if(str.empty()) return std::move(ret);
    // check str is start with '/',
    // if true read first word as node name, else discory all exist node.
    if(str[0] == '/')
    {
        std::size_t pos = str.find_first_of('/', 1);
        auto node = str.substr(0, pos);
        if(node[0] == '/')
            node = node.substr(1);

        auto key = str.substr(pos + 1);
        ret.first = node;
        ret.second = key;
    }
    else
    {
        ret.second = str;
    }

    return std::move(ret);
}
void ShmObjManager::clear_invalid_shm_(const std::string &node)
{
    std::regex node_regex("[0-9]+");
    const std::string check_root_path = ShmManager::get_node_root_path(node);
    if(not std::filesystem::exists(check_root_path)) return;
    for(auto& p : std::filesystem::directory_iterator(check_root_path))
    {
        if(p.is_regular_file() and std::regex_match(p.path().filename().string(), node_regex))
        {
            // get file name
            auto file_name = p.path().string();
            if(not FileMapObject<ShmSystemSharedInfo>::is_valid(file_name))
            {
                auto ctl = ShmManager::get_ctl_key_file(node, p.path().filename().string());
                auto data = ShmManager::get_data_key_file(node, p.path().filename().string());
                std::filesystem::remove(ctl);
                std::filesystem::remove(data);
                continue;
            }

            auto shared_info = std::make_shared<FileMapObject<ShmSystemSharedInfo>>(file_name);

            auto shm_manager = std::make_shared<ShmManager>(node,
                                                            (*shared_info)->self_pid,
                                                            (*shared_info)->shm_mem_ptr,
                                                            (*shared_info)->shm_mem_data_ptr);

            if(not shm_manager->mem().is_busy())
            {
                shm_manager.reset();
                shared_info.reset();
                continue;
            }
        }
    }
}
ShmObjManager::ShmSystemSharedInfo::ShmSystemSharedInfo(const pid_t pid, void *mem_ptr, void* data_ptr,
    ShmMap<ShmString, ObjectInfo*> *map
    , ShmMap<ShmString, ObjectInfo*>* removed_map
    , ProcessMutex* mutex)
  : self_pid(pid)
  , shm_mem_ptr(mem_ptr)
  , shm_mem_data_ptr(data_ptr)
  , shm_obj_map(map)
  , shm_obj_removed_map(removed_map)
  , shm_obj_map_mutex(mutex)
{ }

ShmObjManager::ShmSystemSharedInfo::~ShmSystemSharedInfo() { }

ShmObjManager::ObjectInfo::ObjectInfo(SharedObject &&obj, const std::string &type, uint64_t hash, const uint64_t release_func)
  : obj_ptr(std::move(obj))
  , obj_mutex(ProcessMutex())
  , ref_cnt(1)
  , is_destroy(false)
  , type(type)
  , hash_code(hash)
  , created_pid(getpid())
  , release_func_addr(release_func)
{
}

ShmObjManager::ObjectInfo *ShmObjManager::ObjectInfo::create(SharedObject &&obj, const std::string &type, const uint64_t hash, const uint64_t release_func)
{
    ShmObjManager::Allocator<ObjectInfo> alloc;
    auto p = alloc.allocate(1);
    alloc.construct(p, std::move(obj), type, hash, release_func);
    return p;
}

void ShmObjManager::ObjectInfo::destroy(ObjectInfo *info)
{
    ShmObjManager::Allocator<ObjectInfo> alloc;
    alloc.destroy(info);
    alloc.deallocate(info, 1);
}

ShmObjManager::SharedObject::SharedObject() : ptr(nullptr) { }

ShmObjManager::SharedObject::SharedObject(SharedObject &&rhs)
  : ptr(std::move(rhs.ptr))
{ }

ShmObjManager::ExtNodeMapItem::ExtNodeMapItem()
  : shared_info(nullptr), shm_manager(nullptr)
{
}

ShmObjManager::ExtNodeMapItem::ExtNodeMapItem(std::shared_ptr<FileMapObject<ShmSystemSharedInfo>> &&i, std::shared_ptr<ShmManager> &&shm)
  : shared_info(std::move(i)), shm_manager(std::move(shm))
{
}

ShmObjManager::SharedObjectRetentionBase::SharedObjectRetentionBase(const std::string &node_name, ObjectInfo *info)
  : node_name_(node_name)
  , ptr_(info)
  , manager_(ShmObjManager::instance_())
{
}

ShmObjManager::SharedObjectRetentionBase::~SharedObjectRetentionBase()
{ }

INotifyMonitor::INotifyMonitor(const std::function<void(const struct inotify_event &)> &callback)
  : inotify_fd_(inotify_init())
  , callback_(callback)
{
    std::thread([this]{
        char buffer[1024];
        while (coin::ok())
        {
            // read event
            char buffer[1024];
            ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));
            if (len < 0) { perror("read inotify"); }

            // process event
            for (int i = 0; i < len; ) {
                struct inotify_event *event = reinterpret_cast<struct inotify_event *>(buffer + i);
                i += sizeof(struct inotify_event) + event->len;
                if(callback_) { callback_(*event); }
            }
        }
    }).detach();
}

INotifyMonitor::~INotifyMonitor()
{
}

RetBool INotifyMonitor::add(const std::string &file, uint32_t mask)
{
    auto itor = inotify_map_.find(file);
    if(itor != inotify_map_.end())
    {
        return {false, fmt::format("<{}> already exists", file)};
    }
    auto wd = inotify_add_watch(inotify_fd_, file.c_str(), mask);
    if(wd < 0)
    {
        return {false, fmt::format("add watch failed: {}, {}", file, strerror(errno))};
    }
    inotify_map_.emplace(file, wd);
    inotify_map_rev_.emplace(wd, file);
    return {true};
}

RetBool INotifyMonitor::remove(const std::string &file)
{
    auto itor = inotify_map_.find(file);
    if(itor == inotify_map_.end())
    {
        return {false, fmt::format("<{}> not exists", file)};
    }
    auto wd = itor->second;
    inotify_rm_watch(inotify_fd_, wd);
    inotify_map_.erase(itor);
    inotify_map_rev_.erase(wd);
    return {true};
}

RetBool INotifyMonitor::remove(const int wd)
{
    auto itor = inotify_map_rev_.find(wd);
    if(itor == inotify_map_rev_.end())
    {
        return {false, fmt::format("watch <{}> not exists", wd)};
    }
    auto file = itor->second;
    inotify_rm_watch(inotify_fd_, wd);
    inotify_map_.erase(file);
    inotify_map_rev_.erase(wd);
    return {true};
}

std::string INotifyMonitor::find(const int &wd)
{
    auto itor = inotify_map_rev_.find(wd);
    if(itor != inotify_map_rev_.end())
    {
        return itor->second;
    }
    return std::string();
}

int INotifyMonitor::find(const std::string &file)
{
    auto itor = inotify_map_.find(file);
    if(itor != inotify_map_.end())
    {
        return itor->second;
    }
    return -1;
}

} // namespace coin::data
