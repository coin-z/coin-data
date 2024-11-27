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

#include <tuple>
#include <regex>
#include <mutex>
#include <thread>

#include <sys/types.h>
#include <sys/inotify.h>

#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/crc32.hpp>
#include <coin-data/local/impl/shmobj_manager.hpp>


namespace coin::data
{
CoinNode::CoinNode(const std::string& name) : ThisNode(name)
  , node_shm_root_path_(shm_path(node_path()))
{
    if(not std::filesystem::exists(node_shm_root_path_))
    {
        std::filesystem::create_directories(node_shm_root_path_);
    }
    add_event("update_status");
    add_event("create_object");
    add_event("destroy_object");
}
CoinNode::~CoinNode()
{

}
const std::string CoinNode::create_object_key() const
{
    return "create_object";
}
const std::string CoinNode::destroy_object_key() const
{
    return "destroy_object";
}
std::string CoinNode::shm_path(const std::string &node_path)
{
    return node_path + "/shm/";
}
void CoinNode::notify_update_status()
{
    notify("update_status");
}
void CoinNode::notify_create_object()
{
    notify(create_object_key());
}
void CoinNode::notify_destroy_object()
{
    notify(destroy_object_key());
}

ShmObjManager::ShmObjManager()
  : self_pid_(getpid())
{ }
ShmObjManager::~ShmObjManager()
{
    ProcessLockArea<ProcessMutex> shm_obj_map_lock(
        *(*self_node_->second.shared_info)->shm_obj_map_mutex, [this]
    {
        // check shm memory state
        if(not self_node_->second.shm_manager->mem().is_busy())
        {
            // release object info
            auto& map = *(*self_node_->second.shared_info)->shm_obj_map;
            map.clear();
            shm_obj_map_size_.store(0);
        }
    });
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
    ShmManager::update_shm_addr(node);
    // Step0. clear shm file that not available
    clear_invalid_shm_(node);
    coin::node::NodeMonitor::monitor().init<CoinNode>(node);
    this_node = std::static_pointer_cast<CoinNode>(coin::node::NodeMonitor::monitor().this_node());

    set_node_shm_root_path(this_node->shm_root_path());

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
            __inner::ShmMemory::page_size(),
            self_pid_,
            shm_manager->mem().ctl_addr(),
            shm_manager->mem().data_addr(),
            shm_obj_map_ptr, remove_map_ptr, mutex_ptr
        );
    auto shared_info = shared_info_;

    // Step4. add node info to node_map_
    {
        std::lock_guard<std::mutex> lock(node_map_mutex_);
        // node_map_.emplace(node_name_, ExtNodeMapItem(std::move(shared_info), std::move(shm_manager)));
        // self_node_ = node_map_.find(node_name_);
        self_node_ = std::make_shared<std::pair<std::string, ExtNodeMapItem>>( \
            node_name_, ExtNodeMapItem(std::move(shared_info), std::move(shm_manager))
        );
    }
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
        auto& obj_map = (*self_node_->second.shared_info)->shm_obj_map;
        auto it = obj_map->find(from_std_string(name));
        if(it != obj_map->end())
        {
            return true;
        }
    }
    return false;
}

auto ShmObjManager::make_shared_shm_manager_(const std::string& file_name, const std::string& node, const std::string& key)
-> std::pair<std::shared_ptr<FileMapObject<ShmObjManager::ShmSystemSharedInfo>>, std::shared_ptr<ShmManager>>
{
    auto shared_info = std::make_shared<FileMapObject<ShmObjManager::ShmSystemSharedInfo>>(file_name);
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
bool ShmObjManager::discovery_shm_(const std::string &node, const std::string &key, std::shared_ptr<SharedObjectRetentionBase>& ptr)
{
    bool ret = false;
    // find wheather node exist
    auto node_path = node::NodeMonitor::node_path(node);
    // check node_path exist
    if(not std::filesystem::exists(node_path))
    {
        coin::Print::debug("node path not exist: {}", node_path);
        return ret;
    }
    // calculate shm path and check shm path exist
    auto shm_path = CoinNode::shm_path(node_path);
    if(not std::filesystem::exists(shm_path))
    {
        coin::Print::debug("shm path not exist: {}", shm_path);
        return ret;
    }
    coin::Print::debug("lookup node path exist: {}", shm_path);

    auto node_pid = node::NodeMonitor::node_pid(node);
    auto node_pid_str = std::to_string( node_pid );
    auto shm_file = ShmManager::get_ctl_key_file(node, node_pid_str);
    auto shm_data_file = ShmManager::get_data_key_file(node, node_pid_str);
    if(not std::filesystem::exists(shm_file) or not std::filesystem::exists(shm_data_file))
    {
        coin::Print::debug("shm file not exist: {}", shm_file);
        return ret;
    }

    auto& file_name = shm_file;

    // check node is alive, if not alive, jump to next
    pid_t& pid = node_pid;
    coin::Print::debug("check process alive pid: {}", pid);
    if(kill(pid, 0) != 0)
    {
        coin::Print::debug("<{}:{}> is not alive, remove resource and jump it.", node, pid);
        // remove resource
        auto& ctl = shm_file;
        auto& data = shm_data_file;

        auto shared_shm_manager = make_shared_shm_manager_(file_name, node, key);

        std::filesystem::remove(ctl);
        std::filesystem::remove(data);
        return ret;
    }

    // get file name
    if(not FileMapObject<ShmSystemSharedInfo>::is_valid(file_name))
    {
        // file of shm is not valid anymore, no one can use it, just remove it here.
        auto ctl = shm_file;
        auto data = shm_data_file;
        std::filesystem::remove(ctl);
        std::filesystem::remove(data);

        coin::Print::debug("{} is not valid, remove {} and {}", file_name, ctl, data);

        return ret;
    }

    coin::Print::debug("discover node file: {}", file_name);
    std::pair<std::shared_ptr<FileMapObject<ShmSystemSharedInfo>>, std::shared_ptr<ShmManager>> shared_shm_manager;

    auto node_itor = node_map_.find(node);
    if(node_itor == node_map_.end())
    {
        auto msg = fmt::format("discroy shm node: {}, key: {} not exist in node map", node, key);
        throw std::runtime_error(msg);
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
        return ret;
    }

    ProcessLockArea<ProcessMutex> shm_obj_map_lock(*(*shared_shm_manager.first)->shm_obj_map_mutex,
    [&ptr, &key, &node, &node_itor, &shared_shm_manager, &ret, this]
    {
        auto& obj_map = (*shared_shm_manager.first)->shm_obj_map;
        auto itor = obj_map->find(ShmString(key));
        if(itor != obj_map->end())
        {
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
            coin::Print::debug("size of shm obj map: {}, not find in node filesystem: /{}/{}",
                obj_map->size(), node, key);
        }
    }
    );
    return ret;
}
void ShmObjManager::clear_shm_obj_remove_map_(ExtNodeMapItem& node)
{
    auto& obj_removed_map = (*node.shared_info)->shm_obj_removed_map;
    // check removed map item, if not reference by outside, release it.
    for(auto item = obj_removed_map->begin(); item != obj_removed_map->end();)
    {
        // item->second->obj_mutex.lock();
        pid_t holder_pid = item->second->obj_mutex.holder();
        // item->second->obj_mutex.unlock();

        coin::Print::debug("release object <{}> reference count: {}, is destroy: {}, holder pid: {}",
            item->first, item->second->ref_cnt.load(), item->second->is_destroy, holder_pid);

        if(item->second->ref_cnt.load() == 0 and holder_pid == 0)
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
    // read key from pid file
    // TODO: process key value
    const std::string check_root_path = ShmManager::get_node_root_path(node, "");
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
void ShmObjManager::on_node_create_(const std::string& node, const std::string& event, std::any& obj)
{
    coin::Print::info("create node: {}", node);
}
void ShmObjManager::on_node_destroy_(const std::string& node, const std::string& event, std::any& obj)
{
    coin::Print::info("destroy node: {}", node);
    std::lock_guard<std::mutex> lock(node_map_mutex_);
    auto node_itor = node_map_.find(node);
    if(node_itor != node_map_.end())
    {
        if(not node_itor->second.shm_manager or not node_itor->second.shared_info)
        {
            coin::Print::debug("node {} is not setup yet.", node_itor->first);
        }
        else
        {
            pid_t node_pid = (*node_itor->second.shared_info)->self_pid;
            coin::Print::debug("process {} is not alive.", node_pid);
            auto& obj_list = node_itor->second.obj_list;
            for(auto obj_itor = obj_list.begin(); obj_itor != obj_list.end(); ++obj_itor)
            {
                auto release_shared_obj_ = [&obj_itor, &node_itor, &obj_list]{
                    obj_itor->second->shared_info.reset();
                    obj_itor->second->shm_manager.reset();
                    obj_itor->second->ptr_->ref_cnt.fetch_sub(1);
                    obj_itor->second->ptr_->is_destroy = true;
                };
                ProcessLockArea<ProcessMutex> obj_lock(obj_itor->second->ptr_->obj_mutex,
                    release_shared_obj_);
            }

            node_itor->second.obj_list.clear();
            node_itor->second.obj_removed_list.clear();
            node_itor->second.shared_info.reset();
            node_itor->second.shm_manager.reset();
        }
    }
}
void ShmObjManager::on_object_create_(const std::string& node, const std::string& event, std::any& obj)
{
    coin::Print::debug("create object event: {} -> {}", node, event);
    const std::string& node_name = node;
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
                coin::Print::debug("object <{}> not found in node <{}>", obj_itor->first, node_name);
                ++obj_itor;
            }
        }
    }
    else
    {
        coin::Print::debug("node <{}> not found in node map.", node_name);
    }
    coin::Print::debug("on object create done.");
}
void ShmObjManager::on_object_destroy_(const std::string& node, const std::string& event, std::any& obj)
{
    coin::Print::debug("on destroy object event: {} -> {}", node, event);
    const std::string& node_name = node;
    std::lock_guard<std::mutex> node_map_lock(node_map_mutex_);
    auto node_itor = node_map_.find(node_name);
    if(node_itor != node_map_.end())
    {
        auto& shm_obj_map_mutex = *(*node_itor->second.shared_info)->shm_obj_map_mutex;
        ProcessLockArea<ProcessMutex> shm_obj_map_lock_area(shm_obj_map_mutex, [this, &node_itor, &node_name]
        {
            auto& obj_list = node_itor->second.obj_list;
            for(auto obj_itor = obj_list.begin(); obj_itor != obj_list.end();)
            {
                if(obj_itor->second->ptr_->is_destroy)
                {
                    ProcessLockArea<ProcessMutex> obj_lock(obj_itor->second->ptr_->obj_mutex,
                    [&obj_itor, &node_itor] {
                        obj_itor->second->ptr_->ref_cnt.fetch_sub(1);
                    });
                    coin::Print::debug("object <{}> in node <{}> is destroyed, remove it from list.", obj_itor->first, node_name);
                    node_itor->second.obj_removed_list.emplace(obj_itor->first, obj_itor->second);
                    obj_itor = obj_list.erase(obj_itor);
                }
                else
                {
                    ++obj_itor;
                }
            }
        });
    }
    else
    {
        coin::Print::debug("node <{}> not found in node map.", node_name);
    }

    coin::Print::debug("on object destroy done.");
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
} // namespace coin::data
