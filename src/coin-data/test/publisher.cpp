#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::data::init(argc, argv);

    pid_t pid = getpid();
    auto cnt = coin::data::make_shared_obj<uint64_t>(0);
    auto publisher = coin::data::publisher<uint64_t>("test/uint64_t");
    
    while(coin::ok())
    {
        (*cnt) = std::chrono::system_clock::now().time_since_epoch().count();
        publisher->publish(cnt);
        coin::Print::info("{} pub message: {}", pid, *cnt);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
