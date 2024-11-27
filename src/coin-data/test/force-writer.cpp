#include "local/impl/shmobj_manager.hpp"
#include <thread>
#include <unistd.h>
#include <coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::data::init(argc, argv);

    int cnt = 0;
    pid_t pid = getpid();
    // auto counter = coin::data::make_shared_obj<int>(0);
    int counter = 0;
    coin::data::ShmObjManager::instance().create<int>("qweqwe", 10);

    auto w1 = std::thread([&cnt, &counter](){
        coin::Print::info("start writer.");
        auto writer = coin::data::writer<int>("int");
        while(coin::ok() && cnt < 10)
        {
            (counter) ++;
            coin::Print::info("writer: {}", counter);
            if((counter) >= 10)
            {
                (counter) = 0;
                coin::Print::info("writer: {}", counter);
                writer.reset();
                writer = coin::data::writer<int>("int");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            writer->write(counter);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            coin::data::ShmObjManager::instance().create<int>("qweqwe123", 10);
        }
        coin::Print::info("writer thread exit.");
    });

    auto w2 = std::thread([&cnt, &pid](){
        coin::Print::info("start reader.");
        auto reader = coin::data::reader<int>("/force-test/int");
        while(coin::ok() && cnt < 10)
        {
            reader->lock([&reader, &pid](){
                coin::Print::info("{} reader: {}", pid, reader->read());
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            coin::data::ShmObjManager::instance().destroy<int>("qweqwe123");
        }
        coin::Print::info("reader thread exit.");
    });


    w1.join();
    w2.join();

    return 0;
}
