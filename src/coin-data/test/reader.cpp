#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::data::init(argc, argv);

    pid_t pid = getpid();
    auto reader = coin::data::reader<coin::data::ShmString>("/writer/test-writer/string");

    while(coin::ok())
    {
        reader->lock([&reader, &pid](){
            coin::Print::info("{} read: {}", pid, reader->read());
        });
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
