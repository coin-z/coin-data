/**
 * @file shmobj_manager.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <deque>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <filesystem>

#include <coin-commons/utils/type.hpp>
#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/datetime.hpp>
#include <coin-commons/utils/file_lock.hpp>

#include <coin-data/local/impl/allocator.hpp>
#include <coin-data/local/impl/shm_memory.hpp>
#include <coin-data/local/impl/shm_manager.hpp>
#include <coin-data/local/impl/shm_shared_ptr.hpp>

namespace coin::data
{

class INotifyMonitor
{
public:
    INotifyMonitor(const std::function<void(const struct inotify_event&)>& callback);
    ~INotifyMonitor();

    RetBool add(const std::string& file, uint32_t mask);
    RetBool remove(const std::string& file);
    RetBool remove(const int wd);

    std::string find(const int& wd);
    int find(const std::string& file);

private:
    int inotify_fd_;
    std::function<void(const struct inotify_event&)> callback_;
    std::map<std::string, int> inotify_map_;
    std::map<int, std::string> inotify_map_rev_;
};

class ShmObjManager
{
    template <typename T>
    using Allocator = coin::data::__inner::Allocator<T, ShmObjManager>;
public:
    using ShmString = std::basic_string<char, std::char_traits<char>, Allocator<char>>;
    template<typename EleT>
    using ShmVector = std::vector<EleT, Allocator<EleT>>;

    template<typename EleT>
    using ShmList = std::list<EleT, Allocator<EleT>>;

    template<typename EleT>
    using ShmDeque = std::deque<EleT, Allocator<EleT>>;

    template<typename KeyT, typename ValT, typename _Compare = std::less<KeyT>>
    using ShmMap = std::map<KeyT, ValT, _Compare, Allocator< std::pair<const KeyT, ValT> > >;

    template<typename KeyT, typename ValT, typename _Hash = std::hash<KeyT>, typename _Pred = std::equal_to<KeyT>>
    using ShmUnorderedMap = std::unordered_map<KeyT, ValT,
        _Hash, _Pred, Allocator< std::pair<const KeyT, ValT> > >;

    template<typename KeyT, typename _Compare = std::less<KeyT>>
    using ShmSet = std::set<KeyT, _Compare, Allocator<KeyT> > ;

    inline static std::string to_std_string(const ShmString& str)
    {
        std::string s;
        s.resize(str.size());
        memcpy(s.data(), str.data(), str.size());
        return s;
    }
    inline static ShmString from_std_string(const std::string& str)
    {
        ShmString s;
        s.resize(str.size());
        memcpy(s.data(), str.data(), str.size());
        return s;
    }

    using ProcessMutex = __inner::ShmMemory::ProcessMutex;
    using ProcessRWMutex = __inner::ShmMemory::ProcessRWMutex;

    template<typename MT>
    using ProcessLockGuard = __inner::ShmMemory::ProcessLockGuard<MT>;
    template<typename MT>
    using ProcessRLockGuard = __inner::ShmMemory::ProcessRLockGuard<MT>;
    template<typename MT>
    using ProcessWLockGuard = __inner::ShmMemory::ProcessWLockGuard<MT>;

    template<typename MT>
    using ProcessLockArea = __inner::ShmMemory::ProcessLockArea<MT>;

    template<typename T>
    using ShmSharedPtr = __inner::ShmSharedPtr<T, Allocator<T>, ProcessMutex>;
    
    template<typename T, typename... ArgsT>
    static ShmSharedPtr<T> makeShmShared(ArgsT&&... args)
    {
        return __inner::makeShmShared<T, Allocator<T>, ProcessMutex>(std::forward<ArgsT>(args)...);
    }

public:
    ~ShmObjManager();
    // DO NOT COPY
    ShmObjManager(const ShmObjManager&) = delete;
    ShmObjManager& operator = (const ShmObjManager&) = delete;

    // DO NOT MOVE
    ShmObjManager(ShmObjManager&&) = delete;
    ShmObjManager& operator = (ShmObjManager&&) = delete;

    static ShmObjManager& instance();

    void init(const std::string& key);

    const std::size_t object_map_size() const;

    uint64_t getSharedObjMapUpdateTime();

    bool hasSharedObject(const std::string& name);

    __inner::ShmMemory& mem();
private:
    static std::shared_ptr<ShmObjManager> instance_();
private:
    #pragma pack(4)
    struct SharedObject
    {
        SharedObject();
        ~SharedObject() { }
        SharedObject(SharedObject&& rhs);
        SharedObject& operator = (SharedObject&&) = delete;
        // DO NOT COPY
        SharedObject(const SharedObject&) = delete;
        SharedObject& operator = (const SharedObject&) = delete;

        template<typename T, typename... ArgsT>
        T* make(ArgsT&&...args)
        {
            // create object
            Allocator<T> alloc;
            ptr = alloc.allocate(1);
            alloc.template construct<T>(static_cast<T*>(ptr), std::forward<ArgsT>(args)...);

            return static_cast<T*>(ptr);
        }

        template<typename T>
        static void release(SharedObject& obj)
        {
            Allocator<T> alloc;
            alloc.template destroy<T>(static_cast<T*>(obj.ptr));
            alloc.deallocate(static_cast<T*>(obj.ptr), 1);
            obj.ptr = nullptr;
        }

        void* ptr;
    };
    struct ObjectInfo
    {
        SharedObject obj_ptr;
        ProcessMutex obj_mutex;
        std::atomic<uint64_t> ref_cnt;
        bool is_destroy;
        ShmString type;
        uint64_t hash_code;
        const pid_t created_pid;
        const uint64_t release_func_addr;
        ~ObjectInfo() = default;

        ObjectInfo(SharedObject&& obj, const std::string& type, uint64_t hash, const uint64_t release_func);

        //  DO NOT COPY
        ObjectInfo(const ObjectInfo&) = delete;
        ObjectInfo& operator = (const ObjectInfo&) = delete;
        
        // DO NOT MOVE
        ObjectInfo(ObjectInfo&&) = delete;
        ObjectInfo& operator = (ObjectInfo&&) = delete;

        // object info create and destroy
        static ObjectInfo* create(SharedObject&& obj, const std::string& type, const uint64_t hash, const uint64_t release_func);
        static void destroy(ObjectInfo* info);
    };
    #pragma pack()
    struct ShmSystemSharedInfo {
        // process pid of this shm owner
        pid_t self_pid;
        // shared memory pointer, point to shm memroy
        void* shm_mem_ptr;
        void* shm_mem_data_ptr;
        // object map, index object in shm memory by key
        ProcessMutex* shm_obj_map_mutex;
        ShmMap<ShmString, ObjectInfo*>* shm_obj_map;
        ShmMap<ShmString, ObjectInfo*>* shm_obj_removed_map;

        // last update time of shm object map, ns
        std::atomic_int64_t update_time = 0;

        ShmSystemSharedInfo(const pid_t pid, void* mem_ptr, void* data_ptr, 
                                ShmMap<ShmString, ObjectInfo*>* map,
                                ShmMap<ShmString, ObjectInfo*>* removed_map,
                                ProcessMutex* mutex);
        ~ShmSystemSharedInfo();

        // DO NOT COPY
        ShmSystemSharedInfo(const ShmSystemSharedInfo&) = delete;
        ShmSystemSharedInfo& operator = (const ShmSystemSharedInfo&) = delete;

        // DO NOT MOVE
        ShmSystemSharedInfo(ShmSystemSharedInfo&&) = delete;
        ShmSystemSharedInfo& operator = (ShmSystemSharedInfo&&) = delete;
        ShmSystemSharedInfo() = delete;
    };
public:
    class SharedObjectRetentionBase
    {
        friend class ShmObjManager;
    protected:
        std::string node_name_;
        ObjectInfo* ptr_;
        std::mutex lock_;
    public:
        std::weak_ptr<FileMapObject<ShmSystemSharedInfo>> shared_info;
        std::weak_ptr<ShmManager> shm_manager;

        SharedObjectRetentionBase(const std::string& node_name, ObjectInfo* info);
        virtual ~SharedObjectRetentionBase();

        // DO NOT COPY
        SharedObjectRetentionBase(const SharedObjectRetentionBase&) = delete;
        SharedObjectRetentionBase& operator = (const SharedObjectRetentionBase&) = delete;
    private:
        std::shared_ptr<ShmObjManager> manager_;
    };
    template<typename T>
    class SharedObjectRetention : public SharedObjectRetentionBase
    {
        friend class ShmObjManager;
    public:
        SharedObjectRetention() = default;
        SharedObjectRetention(SharedObjectRetention&& rhs) = default;

        SharedObjectRetention(const std::string& node_name, ObjectInfo* info)
          : SharedObjectRetentionBase(node_name, info) {}

        virtual ~SharedObjectRetention() override final;

        // DO NOT COPY
        SharedObjectRetention(const SharedObjectRetention&) = delete;
        SharedObjectRetention& operator = (const SharedObjectRetention&) = delete;
    };
    template<typename T>
    class SharedObjectRetentionReference : public SharedObjectRetentionBase
    {
        friend class ShmObjManager;
    public:
        SharedObjectRetentionReference() = default;
        SharedObjectRetentionReference(SharedObjectRetentionReference&& rhs) = default;
        SharedObjectRetentionReference(const std::string& node_name, ObjectInfo* info)
          : SharedObjectRetentionBase(node_name, info) {}

        virtual ~SharedObjectRetentionReference() override final;

        // DO NOT COPY
        SharedObjectRetentionReference(const SharedObjectRetentionReference&) = delete;
        SharedObjectRetentionReference& operator = (const SharedObjectRetentionReference&) = delete;
    };
    template<typename T>
    using SharedObjectSharedPtr = std::shared_ptr<SharedObjectRetention<T>>;
    template<typename T>
    using SharedObjectWeakPtr = std::weak_ptr<SharedObjectRetention<T>>;
    template<typename T>
    static T& shared_obj(SharedObjectSharedPtr<T>& ptr)
    {
        return *reinterpret_cast<T*>(ptr->ptr_->obj_ptr.ptr);
    }
private:
    ShmObjManager();

    const pid_t self_pid_;
    std::string node_name_;

    using SharedObjectRetentionMap = std::map<std::string, std::shared_ptr<SharedObjectRetentionBase>>;
    // shared memory manager and shared info of this node
    struct ExtNodeMapItem
    {
        std::shared_ptr<FileMapObject<ShmSystemSharedInfo>> shared_info;
        std::shared_ptr<ShmManager> shm_manager;
        SharedObjectRetentionMap obj_list;
        SharedObjectRetentionMap obj_removed_list;

        ExtNodeMapItem();
        ExtNodeMapItem(std::shared_ptr<FileMapObject<ShmSystemSharedInfo>>&& i, std::shared_ptr<ShmManager>&& shm);
    };
    // all node that discory by this node, will insert to node_map_
    // shared object will insert to obj_list or obj_remove_list
    std::mutex node_map_mutex_;
    std::map<std::string, ExtNodeMapItem> node_map_;
    std::map<std::string, ExtNodeMapItem>::iterator self_node_;
    std::shared_ptr<ShmManager> shm_manager_;
    std::shared_ptr<FileMapObject<ShmSystemSharedInfo>> shared_info_;

    std::atomic_uint64_t shm_obj_map_size_;
    std::map<std::string, std::unique_ptr< std::function<void(void*) >>> shm_obj_release_map_;

    // record node map for monitor, not online for now
    std::unique_ptr<INotifyMonitor> inotify_monitor_;

    static void clear_invalid_shm_(const std::string& node);

    bool discovery_ext_map_(const std::string& node, const std::string& key, std::shared_ptr<SharedObjectRetentionBase>& ptr);
    bool discovery_shm_(const std::string& node, const std::string& key, std::shared_ptr<SharedObjectRetentionBase>& ptr);

    // node monitor task, work in a separate thread
    void node_monitor_();
    // shared object monitor task, work in a separate thread
    void inotify_shared_object_monitor_(const struct inotify_event& event);

    template<typename T>
    static bool lock_object_(SharedObjectSharedPtr<T>& data, const std::function<void()>& func);

    static void clear_shm_obj_remove_map_(ExtNodeMapItem& node);

public:
    template<typename T, typename... ArgsT>
    SharedObjectSharedPtr<T> create(const std::string& name, ArgsT&&... args);
    template<typename T>
    bool destroy(const std::string& name);

    template<typename T>
    SharedObjectSharedPtr<T> discovery(const std::string& name);
    template<typename T>
    bool release(const std::string& name);

    template<typename T, typename... ArgsT>
    SharedObjectSharedPtr<T> make(ArgsT&&... args);

    template<typename T>
    RetBool reset(SharedObjectSharedPtr<T>& obj);

    template<typename T>
    static bool lock_object(SharedObjectSharedPtr<T>& data, const std::function<void()>& func);

private:
    static std::pair<std::string, std::string> read_node_idx_(const std::string& str);
};

template <typename T>
inline bool ShmObjManager::lock_object_(SharedObjectSharedPtr<T> &data, const std::function<void()> &func)
{
    bool ret = false;
    std::lock_guard<std::mutex> data_lock(data->lock_);
    auto shared_info = data->shared_info.lock();
    auto shm_manager = data->shm_manager.lock();
    if(shared_info and shm_manager and data->ptr_)
    {
        ProcessLockArea<ProcessMutex> lock_area(data->ptr_->obj_mutex,
        [&data, &func, &ret]{
            if(not data->ptr_->is_destroy)
            {
                func();
                ret = true;
            }
        });
    }
    return ret;
}
template <typename T, typename... ArgsT>
inline ShmObjManager::SharedObjectSharedPtr<T> ShmObjManager::create(const std::string &name, ArgsT &&...args)
{
    /**
     * @brief create a shared object with type T, args will be passed to T's constructor
     *     this shared object will be owned by this node, and will be destroyed by this node
     *     it will be inserted into shm_obj_map_
     *     SharedObjectSharedPtr will be returned, it is a shared_ptr of SharedObjectRetention<T>
     *     SharedObjectRetention<T> holds a pointer to SharedInfo,
     *     SharedInfo holds all information of this shared object in shm
     */
    SharedObjectSharedPtr<T> ret;
    coin::Print::info("create object: {}", name);

    std::lock_guard<std::mutex> lock(node_map_mutex_);

    // find object in obj list, if exist then return, else create and insert
    auto obj_itor = self_node_->second.obj_list.find(name);
    if(obj_itor != self_node_->second.obj_list.end())
    {
        ret = std::dynamic_pointer_cast<SharedObjectRetention<T>>(obj_itor->second);
    }
    else
    {
        // create a lock area, lock area will forgive the lock code when mutex's owener is exist.
        ProcessLockArea<ProcessMutex> shm_obj_map_lock_area(
        *(*self_node_->second.shared_info)->shm_obj_map_mutex,
        [&name, &args..., &ret, this] {
            auto& obj_map = (*self_node_->second.shared_info)->shm_obj_map;
            auto it = obj_map->find(from_std_string(name));
            if(it == (*self_node_->second.shared_info)->shm_obj_map->end())
            {
                SharedObject obj;
                obj.make<T>(std::forward<ArgsT>(args)...);
                auto tname = type_name<T>();
                
                auto release_func_itor = shm_obj_release_map_.find(tname);
                if(release_func_itor == shm_obj_release_map_.end())
                {
                    auto release_func = shm_obj_release_map_.emplace(tname,
                    std::make_unique<std::function<void(void*)>>(
                        [](void* p){
                            auto obj = reinterpret_cast<T*>(p);
                            Allocator<T> alloc;
                            alloc.destroy(obj);
                            alloc.deallocate(obj, 1);
                        }
                    ));
                    release_func_itor = release_func.first;
                }

                auto obj_info_item = ObjectInfo::create(
                    std::move(obj), tname, typeid(T).hash_code(), (uint64_t)release_func_itor->second.get());


                auto emplace_ret = obj_map->emplace(ShmString(name), obj_info_item);
                shm_obj_map_size_.store(obj_map->size());
                if(emplace_ret.second)
                {
                    auto& obj_info = emplace_ret.first->second;
                    coin::Print::debug("create object <{}> reference count: {}", emplace_ret.first->first, obj_info->ref_cnt.load());
                    {
                        auto obj_itor = self_node_->second.obj_removed_list.find(name);
                        if(obj_itor != self_node_->second.obj_removed_list.end())
                        {
                            ret = std::static_pointer_cast<SharedObjectRetention<T>>( obj_itor->second );
                            ret->node_name_ = node_name_;
                            ret->ptr_ = (obj_info);
                            self_node_->second.obj_removed_list.erase(obj_itor);
                        }
                        if(not ret)
                        {
                            ret = std::make_shared<SharedObjectRetention<T>>(node_name_, obj_info);
                        }
                        ret->shm_manager = self_node_->second.shm_manager;
                        ret->shared_info = self_node_->second.shared_info;
                        self_node_->second.obj_list.emplace(name, ret);
                    }
                }
                (*self_node_->second.shared_info)->update_time.exchange(coin::DateTime::current_date_time().to_nsecs_since_epoch());

                // touch key file to notify others
                {
                    coin::Print::debug("touch key file: {}", self_node_->second.shm_manager->data_file());
                    int fd = (open(self_node_->second.shm_manager->data_file().c_str(), O_RDONLY, 0600));
                    if(fd > 0) close(fd);
                }
            }
            else
            {
                coin::Print::warn("object {} already exist in shm map, but not in obj map", name);
            }
        });
    }

    return ret;
}
template <typename T>
inline bool ShmObjManager::destroy(const std::string &name)
{
    {
        std::lock_guard<std::mutex> lock(node_map_mutex_);
        auto obj_itor = self_node_->second.obj_list.find(name);
        if(obj_itor != self_node_->second.obj_list.end())
        {
            std::lock_guard<std::mutex> item_lock(obj_itor->second->lock_);
            coin::Print::debug("object {} reference count: {}", name, obj_itor->second.use_count());
            obj_itor->second->shared_info.reset();
            obj_itor->second->shm_manager.reset();
            obj_itor->second->ptr_ = nullptr;
            if(obj_itor->second.use_count() > 1)
            {
                coin::Print::debug("object {} reference count: {}, remember it in removed map.", name, obj_itor->second.use_count());
                self_node_->second.obj_removed_list.emplace(obj_itor->first, obj_itor->second);
            }
            self_node_->second.obj_list.erase(obj_itor);
        }
    }

    ProcessLockArea<ProcessMutex> shm_obj_map_lock_area(*(*self_node_->second.shared_info)->shm_obj_map_mutex, [&name, this]
    {
        auto& obj_removed_map = (*self_node_->second.shared_info)->shm_obj_removed_map;
        auto& obj_map = (*self_node_->second.shared_info)->shm_obj_map;
        coin::Print::debug("destroy from shm object manager: {}", name);

        auto it = obj_map->find(ShmString(name));
        if(it != obj_map->end())
        {
            bool is_destroy = false;
            auto& obj_info = it->second;
            ProcessLockArea<ProcessMutex> obj_lock(obj_info->obj_mutex,
            [&it, &is_destroy, &name, &obj_removed_map, &obj_map, &obj_info, this]
            {
                // mark it as destroy
                obj_info->is_destroy = true;

                // check removed map item, if not reference by outside, release it.
                clear_shm_obj_remove_map_(self_node_->second);

                // release object if reference count == 1, or not move it to removed map
                if(obj_info->ref_cnt.load() == 1)
                {
                    coin::Print::debug("release object <{}> reference count: {}", name, obj_info->ref_cnt.load());
                    SharedObject::release<T>(obj_info->obj_ptr);
                    is_destroy = true;
                }
                else
                {
                    coin::Print::debug("reference count of <{}>: {}, keep it alive.", name, obj_info->ref_cnt.load());
                    auto ts = coin::DateTime::current_date_time().to_nsecs_since_epoch();
                    (*self_node_->second.shared_info)->update_time.exchange(ts);
                    auto rmeoved_obj_key = to_std_string(it->first) + std::to_string(ts);

                    it->second->ref_cnt.fetch_sub(1);
                    obj_removed_map->emplace(from_std_string(rmeoved_obj_key), it->second);
                }
            });
            if(is_destroy)
            {
                ObjectInfo::destroy(it->second);
                coin::Print::debug("destroy object <{}> <{:X}>", name, (size_t)it->second);
            }
            obj_map->erase(it);
            shm_obj_map_size_.store(obj_map->size());

            // touch key file to notify others
            {
                coin::Print::debug("touch key file: {}", self_node_->second.shm_manager->data_file());
                int fd = (open(self_node_->second.shm_manager->data_file().c_str(), O_RDONLY, 0600));
                if(fd > 0) close(fd);
            }
        }
    });
    return true;
}
template <typename T>
inline ShmObjManager::SharedObjectSharedPtr<T> ShmObjManager::discovery(const std::string &name)
{
    // Step0. create ret object, it holds the pointer and node name.
    //     Even Node or Key not exist, it will be created.
    //     When Node or key exist, it will set resource by ShmObjManager.
    // get node name from name
    coin::Print::debug("discovery object: {}", name);
    std::shared_ptr<SharedObjectRetentionBase> ret;

    auto idx = read_node_idx_(name);
    std::string& node = idx.first;
    std::string& key = idx.second;

    std::string node_path = ShmManager::get_root();
    if(not node.empty())
    {
        node_path = ShmManager::get_node_root_path(node);
    }

    std::lock_guard<std::mutex> lock(node_map_mutex_);

    // Step1. discovery object in ext map, if not exist, discovery in shm
    //    if not exist everywhere, remember it in monitor map
    if( discovery_ext_map_(node, key, ret) )
    {
        // Step1.1 discovery in ext map.
    }
    else if( discovery_shm_(node, key, ret) )
    {
        // Step1.2 discovery in shm area.
    }
    else
    {
        // Step1.3 make a null shared object retention ptr,
        //         and add it to removed object list of node
        ret = std::make_shared<SharedObjectRetention<T>>(node, nullptr);
        auto node_itor = node_map_.find(node);
        if(node_itor == node_map_.end())
        {
            auto insert_ret = node_map_.emplace(node, ExtNodeMapItem());
            node_itor = insert_ret.first;
        }
        node_itor->second.obj_removed_list.emplace(key, ret);
    }
    // Step2. add node to monitor
    coin::Print::debug("add monitor: {}", node_path);
    // create node_path if not exist, for monitor
    if(not std::filesystem::exists(node_path))
    {
        std::filesystem::create_directories(node_path);
    }
    inotify_monitor_->add(node_path, IN_CLOSE);
    return std::static_pointer_cast<SharedObjectRetention<T>>(ret);
}

template <typename T>
inline bool ShmObjManager::release(const std::string &name)
{
    return false;
}

template <typename T, typename... ArgsT>
inline ShmObjManager::SharedObjectSharedPtr<T> ShmObjManager::make(ArgsT &&...args)
{
    SharedObjectSharedPtr<T> ret = nullptr;
    ProcessLockArea<ProcessMutex> shm_obj_map_lock_area(
        *(*self_node_->second.shared_info)->shm_obj_map_mutex,
        [&args..., &ret, this] {
            SharedObject obj;
            obj.make<T>(std::forward<ArgsT>(args)...);
            auto tname = type_name<T>();
            
            auto release_func_itor = shm_obj_release_map_.find(tname);
            if(release_func_itor == shm_obj_release_map_.end())
            {
                auto release_func = shm_obj_release_map_.emplace(tname,
                std::make_unique<std::function<void(void*)>>(
                    [](void* p){
                        auto obj = reinterpret_cast<T*>(p);
                        Allocator<T> alloc;
                        alloc.destroy(obj);
                        alloc.deallocate(obj, 1);
                    }
                ));
                release_func_itor = release_func.first;
            }

            auto obj_info_item = ObjectInfo::create(
                std::move(obj), tname, typeid(T).hash_code(), (uint64_t)release_func_itor->second.get()
            );
            ret = std::make_shared<SharedObjectRetention<T>>(node_name_, obj_info_item);
        });
    return ret;
}

template <typename T>
inline RetBool ShmObjManager::reset(SharedObjectSharedPtr<T> &obj)
{
    if(not obj)
    {
        return RetBool(false);
    }
    ProcessLockArea<ProcessMutex> shm_obj_map_lock_area(
        *(*self_node_->second.shared_info)->shm_obj_map_mutex,
        [&obj, this]() {
            SharedObject::release<T>(obj->ptr_->obj_ptr);
            ObjectInfo::destroy(obj->ptr_);

            obj->ptr_ = nullptr;
            obj.reset();
        });
    return RetBool(true);
}

template <typename T>
inline bool ShmObjManager::lock_object(SharedObjectSharedPtr<T> &data, const std::function<void()> &func)
{
    return ShmObjManager::lock_object_(data, func);
}
template <typename T>
using Allocator = ShmObjManager::Allocator<T>;
using ShmString = ShmObjManager::ShmString;
template<typename T>
using ShmVector = ShmObjManager::ShmVector<T>;
template<typename KeyT, typename ValT>
using ShmMap = ShmObjManager::ShmMap<KeyT, ValT>;
template<typename T>
using ShmList = ShmObjManager::ShmList<T>;
template<typename T>
using ShmDeque = ShmObjManager::ShmDeque<T>;
template<typename KeyT, typename ValT>
using ShmUnorderedMap = ShmObjManager::ShmUnorderedMap<KeyT, ValT>;
template<typename T>
using ShmSet = ShmObjManager::ShmSet<T>;

using ProcessMutex = ShmObjManager::ProcessMutex;
template<typename MT>
using ProcessLockGuard = ShmObjManager::ProcessLockGuard<MT>;
template<typename MT>
using ProcessRLockGuard = ShmObjManager::ProcessRLockGuard<MT>;
template<typename MT>
using ProcessWLockGuard = ShmObjManager::ProcessWLockGuard<MT>;

template<typename T>
using SharedObjectSharedPtr = ShmObjManager::SharedObjectSharedPtr<T>;
template<typename T>
using ShmSharedPtr = ShmObjManager::ShmSharedPtr<T>;

inline std::string to_std_string(const ShmString& str)
{
    return ShmObjManager::to_std_string(str);
}
inline ShmString from_std_string(const std::string& str)
{
    return ShmObjManager::from_std_string(str);
}

template<typename T, typename... ArgsT>
ShmSharedPtr<T> makeShmShared(ArgsT&&... args)
{
    return __inner::makeShmShared<T, Allocator<T>, ProcessMutex>(std::forward<ArgsT>(args)...);
}
namespace __inner
{
template<typename T>
class SharedCircularBuffer
{
    class InOut
    {
    public:
        InOut(const std::function<void()>& in, const std::function<void()>& out) : out_(out)
        { if(in) in(); }
        ~InOut()
        { if(out_) out_(); }
    private:
        const std::function<void()> out_;
    };
public:
    SharedCircularBuffer() = delete;
    SharedCircularBuffer(const SharedCircularBuffer&) = delete;
    SharedCircularBuffer& operator = (const SharedCircularBuffer&) = delete;
    SharedCircularBuffer(const size_t& size) : head_(0), tail_(0), total_size_(size), buffer_(size)
    {
        buffer_.shrink_to_fit();
    }
    ~SharedCircularBuffer()
    {
        buffer_.clear();
    }

    void push_back(const T& val)
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        InOut io(nullptr, [this](){
            tail_ += 1;
            if(tail_ - head_ >= total_size_)
            {
                head_ += 1;
            }
        });
        size_t idx = (tail_) % total_size_;
        buffer_[idx] = val;
    }

    [[nodiscard]] T& operator [] (const size_t& idx)
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        auto i = idx;
        if(i < head_)
        {
            i = head_;
        }
        else if(i >= tail_)
        {
            i = (tail_ - 1);
        }
        return buffer_[i % total_size_];
    }

    [[nodiscard]] T copy(const size_t& idx)
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        auto i = idx;
        if(i < head_)
        {
            i = head_;
        }
        else if(i >= tail_)
        {
            i = (tail_ - 1);
        }
        return buffer_[i % total_size_];
    }

    size_t head() noexcept
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        return head_;
    }

    size_t tail() noexcept
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        return tail_;
    }

    void clear() noexcept
    {
        buffer_.clear();
    }
    size_t size() noexcept
    {
        ProcessLockGuard<ProcessMutex> lock(mutex_, 100);
        return buffer_.size();
    }

private:
    size_t head_;
    size_t tail_;
    const size_t total_size_;
    ShmVector<T> buffer_;
    ProcessMutex mutex_;
};
}
template <typename T>
inline ShmObjManager::SharedObjectRetention<T>::~SharedObjectRetention()
{
}
template <typename T>
inline ShmObjManager::SharedObjectRetentionReference<T>::~SharedObjectRetentionReference()
{
    // decreace reference count
    this->info_->ptr_->ref_cnt.fetch_sub(1);
    if(this->info_->ptr_->ref_cnt.load() == 0)
    {
        this->info_->ptr_->is_destroy = true;
    }
}
} // namespace coin::data
