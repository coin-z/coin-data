#include <coin-data/coin-data.hpp>

bool service_callback(const uint64_t& req, uint64_t& res)
{
    res = req * 10;
    return true;
}
int main(int argc, char *argv[])
{
    coin::data::init(argc, argv);

    auto service = coin::data::service<uint64_t, uint64_t>("test/uint64_t",
        std::bind(&service_callback, std::placeholders::_1, std::placeholders::_2));

    while(coin::ok())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
