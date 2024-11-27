#include "coin-data/node/node.hpp"
#include <iostream>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <functional>

#include <fmt/format.h>
#include <fmt/printf.h>

void node_envent_callback(const std::string& name, const std::string& event)
{
    std::cout << name << " " << event << std::endl;
}

int main(int argc, char *argv[])
{
    if(argc == 1)
    {
        fmt::print("no node to monitor.\n");
        return 0;
    }

    auto& monitor = coin::node::NodeMonitor::monitor();
    monitor.init("node-monitor");

    for(int i = 1; i < argc; i++)
    {
        fmt::print("add node <{}> to monitor.\n", argv[i]);
        monitor.add(argv[i], coin::node::NodeMonitor::make_callback_map({
            {"test", std::bind(&node_envent_callback, std::placeholders::_1, std::placeholders::_2)},
            {"destroy", std::bind(&node_envent_callback, std::placeholders::_1, std::placeholders::_2)},
            {"create", std::bind(&node_envent_callback, std::placeholders::_1, std::placeholders::_2)},
        }));
    }

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
