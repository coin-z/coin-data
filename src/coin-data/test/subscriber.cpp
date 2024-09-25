#include "local/impl/shmobj_manager.hpp"
#include <communicator.hpp>
#include <coin-data.hpp>
#include <thread>


int main(int argc, char* argv[])
{
    coin::init(argc, argv);

    // auto subscriber = coin::data::Communicator::subscriber<uint64_t>("uint64_t",
    //     [](const coin::data::Subscriber<uint64_t>::DataPtr& data){
    //         coin::Print::info("subscriber: {}", *data);
    //     });

    auto ptr = coin::data::ShmObjManager::instance().discovery<int>("/publisher/test");
    auto ptr1 = coin::data::ShmObjManager::instance().discovery<int>("/publisher/test");
    auto ptr2 = coin::data::ShmObjManager::instance().discovery<int>("/publisher/test");
    int cnt = 0;
    while(coin::ok())
    {
        // if(ptr)
        // coin::Print::info("used count: {}", ptr.use_count());
        coin::data::ShmObjManager::lock_object(ptr, [&](){
            coin::Print::info("ptr: {}", coin::data::ShmObjManager::shared_obj(ptr));
        });
        // try
        // {
        // }
        // catch(const std::exception& e)
        // {
        //     coin::Print::warn("{}", e.what());
        // }
        coin::spin_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // if(cnt++ > 5) break;
    }
    return 0;
}
