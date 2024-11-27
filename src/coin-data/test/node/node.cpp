#include "coin-data/node/node.hpp"
#include "coin-commons/utils/utils.hpp"
#include <string>
#include <sys/inotify.h>
#include <unistd.h>

#include <fmt/format.h>

class RootNode : public coin::node::ThisNode
{
public:
    RootNode(const std::string& name) : coin::node::ThisNode(name)
    {
    }
    virtual ~RootNode() override
    {
    }
};

int main(int argc, char *argv[])
{
    if(argc == 1)
    {
        fmt::print("input node name.\n");
        return 0;
    }
    const std::string node_name(argv[1]);
    auto& monitor = coin::node::NodeMonitor::monitor();
    monitor.init<RootNode>(node_name);
    monitor.this_node()->add_event("test");

    while(true)
    {
        coin::Print::info(("{} - node {}, notify event: test"), getpid(), node_name);
        monitor.this_node()->notify("test");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
