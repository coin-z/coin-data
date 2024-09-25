#include "coin-commons/utils/utils.hpp"
#include "local/impl/shmobj_manager.hpp"
#include <communicator.hpp>
#include <coin-data.hpp>
#include <thread>
#include <unistd.h>


int main(int argc, char* argv[])
{
    coin::init(argc, argv);

    // auto publisher = coin::data::Communicator::publisher<uint64_t>("uint64_t");
    // auto cnt = coin::data::makeShmShared<uint64_t>(0);
    // while(coin::ok())
    // {
    //     (*cnt) = std::chrono::system_clock::now().time_since_epoch().count();
    //     publisher->publish(cnt);
    //     coin::spin_once();
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }

    int cnt = 0;
    int counter = 0;

    auto w1 = std::thread([&cnt, &counter](){
        coin::Print::info("start writer.");
        auto shared_obj = coin::data::ShmObjManager::instance().create<int>("test", 0);
        coin::Print::info("writer addr of shared obj: {:X}", (uint64_t)shared_obj.get());
        while(coin::ok() && cnt < 10)
        {
            // cnt++;
            counter ++;
            if(counter >= 10)
            {
                counter = 0;
                coin::data::ShmObjManager::instance().destroy<int>("test");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                shared_obj = coin::data::ShmObjManager::instance().create<int>("test", 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            coin::data::ShmObjManager::shared_obj(shared_obj) ++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        coin::data::ShmObjManager::instance().destroy<int>("test");
        coin::Print::info("writer thread exit.");
    });

    auto w2 = std::thread([&cnt](){
        coin::Print::info("start reader.");
        auto shared_obj = coin::data::ShmObjManager::instance().create<int>("test", 0);
        coin::Print::info("reader addr of shared obj: {:X}", (uint64_t)shared_obj.get());
        while(coin::ok() && cnt < 10)
        {
            auto make_obj = coin::data::ShmObjManager::instance().make<int>(10);
            coin::data::ShmObjManager::lock_object(shared_obj, [&](){
                coin::Print::info("read test: {}, pid: {}",
                    coin::data::ShmObjManager::shared_obj(shared_obj), getpid());
            });
            coin::data::ShmObjManager::instance().reset(make_obj);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        coin::Print::info("reader thread exit.");
    });


    w1.join();
    w2.join();

    // abort();

    return 0;
}
