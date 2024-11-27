#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::data::init(argc, argv);

    pid_t pid = getpid();

    auto subscriber = coin::data::subscriber<uint64_t>("/publisher/test/uint64_t",
        [&pid](const coin::data::Subscriber<uint64_t>::DataPtr& data){
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            coin::Print::info("{} subscriber: {} {}", pid, *data, (now - *data) / 1000);
        });

    while(coin::ok())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
