#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::data::init(argc, argv);

    int cnt = 0;
    pid_t pid = getpid();
    
    auto writer = coin::data::writer<coin::data::ShmString>("test-writer/string");

    while(coin::ok())
    {
        auto str = coin::data::from_std_string(fmt::format("{} {}", pid, cnt++));
        writer->write(str);
        coin::Print::info("{} writer: {}", pid, str);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
