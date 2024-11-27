# Coin-Data
A high-performance data distribution library

## Main Framework
![coin-data-framework](.media/coin-data-framework.excalidraw.png)

coin-data provide three common interface to users, `read/write`, `publish/subscribe`, `service/client`.

## Build
1. make `build` directory and change directory to `build`
    ```bash
    mkdir build && cd build
    ```
2. execute `cmake` command
    ```bash
    cmake -DCOIN_ENABLE_TEST=ON -DCMAKE_BUILD_TYPE=Release ..
    ```
3. run `make`
    ```bash
    make -j4
    ```
## Run test sample
1. change directory to `build`
    ```bash
    cd build
    ```
2. start publisher node
    ```bash
    COIN_NODE_NAME=publisher ./src/coin-data/test/coin-data-publisher
    ```
3. start subscriber node
    ```bash
    COIN_NODE_NAME=subscriber ./src/coin-data/test/coin-data-subscriber
    ```

## Code Sample

### 1. Writer/Reader

- writer node code sample

```c++
#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::init(argc, argv);

    int cnt = 0;
    pid_t pid = getpid();
    
    auto writer = coin::data::writer<coin::data::ShmString>("test-writer/string");

    while(coin::ok())
    {
        auto str = coin::data::make_shared_obj<coin::data::ShmString>(fmt::format("{} {}", pid, cnt++));
        writer->write(str);
        coin::Print::info("{} writer: {}", pid, *str);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

- reader node code sample

```c++
#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::init(argc, argv);

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
```

### 2. Publisher/Subscriber

- publisher node code sample

```c++
#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::init(argc, argv);

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
```

- subscriber node code sample

```c++
#include <coin-data/coin-data.hpp>

int main(int argc, char* argv[])
{
    coin::init(argc, argv);

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
```

### 3. Service/Client

- service node code sample

```c++
#include <coin-data/coin-data.hpp>

bool service_callback(const uint64_t& req, uint64_t& res)
{
    res = req * 10;
    return true;
}
int main(int argc, char *argv[])
{
    coin::init(argc, argv);

    auto service = coin::data::service<uint64_t, uint64_t>("test/uint64_t",
        std::bind(&service_callback, std::placeholders::_1, std::placeholders::_2));

    while(coin::ok())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

- client node code sample

```c++
#include <coin-data/coin-data.hpp>

int main(int argc, char *argv[])
{
    coin::init(argc, argv);

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
```
