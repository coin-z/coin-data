#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <thread>


void sig_int(int sig)
{
    std::cout << "signal: " << sig << std::endl;

    // std::thread([=]{
    //    sleep(10);
    //    exit(-10);
    // }).detach();

    std::cout << "out of signal: " << sig << std::endl;
}

int main(int argc, char **argv)
{
    std::cout << "hello world" << std::endl;
    signal(SIGINT, sig_int);
    signal(SIGKILL, sig_int);
    int cnt = 0;
    while(true)
    {
        std::cout << cnt++ << " hello world" << std::endl;
        sleep(1);
    }
    return 0;
}