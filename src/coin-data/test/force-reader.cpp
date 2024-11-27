#include <thread>
#include <unistd.h>
#include <coin-data/coin-data.hpp>


int main(int argc, char* argv[])
{
    auto node_name = "force-reader-" + std::to_string(getpid());
    coin::data::init(argc, argv, node_name);

    int cnt = 0;
    pid_t pid = getpid();
    
    coin::Print::info("start reader.");
    auto reader = coin::data::reader<int>("/force-test/int");
    while(coin::ok() && cnt < 10)
    {
        reader->lock([&reader, &pid](){
            static int cnt = -1;
            if(cnt != reader->read())
            {
                cnt = reader->read();
                coin::Print::info("{} reader: {}", pid, cnt);
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    coin::Print::info("reader thread exit.");

    return 0;
}
