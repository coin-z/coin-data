#include "coin-commons/executor/executor.hpp"
#include <unistd.h>
#include <iostream>

int main(int argc, char *argv[])
{
    auto executor = std::make_shared<coin::Executor>();
    executor->execute("./dummy-test", std::vector<std::string>{"-l"});
    sleep(2);
    std::cout << "executor shutdown..." << std::endl;
    executor->shutdown(5000);
    executor->wait();
    std::cout << "executor exit code: " << executor->exist_code() << std::endl;
    return 0;
}
