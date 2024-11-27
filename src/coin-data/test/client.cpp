#include <coin-data/coin-data.hpp>

int main(int argc, char *argv[])
{
    coin::data::init(argc, argv);

    uint64_t cnt = 0;
    auto client = coin::data::client<uint64_t, uint64_t>("/service/test/uint64_t");

    while(coin::ok())
    {
        client->call(
            [&cnt](uint64_t& req){ req = cnt; },
            [](const uint64_t& ack)
            {
                coin::Print::info("ack: {}", ack);
            }
        );
        cnt += 1;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
