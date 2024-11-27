/**
 * @file communicator.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "communicator.hpp"
#include "coin-commons/utils/utils.hpp"
#include <coin-data/local/impl/shmobj_manager.hpp>
#include <stdlib.h>
#include <uuid/uuid.h>

namespace coin::data
{
MutableCommunicator::MutableCommunicator(const std::string& id) : id_(id)
{
}
Communicator::Communicator()
{
}
Communicator::~Communicator()
{
    coin::Print::warn("Communicator is destructed.");
}
std::unique_ptr<std::map<std::string, MutableCommunicator::Ptr> >&
Communicator::mutable_communicator()
{
    static auto mc = std::make_unique<std::map<std::string, MutableCommunicator::Ptr>>();
    return mc;
}

const std::string& Communicator::node_name()
{
    return coin::data::local::LocalChannal::instance().self_name();
}

Service<Communicator::MutableServerReq, Communicator::MutableServerAck>::Ptr Communicator::mutable_communicator_service_;
void Communicator::init(int argc, char *argv[], const std::string& name)
{
    coin::data::local::LocalChannal::instance();
    coin::data::local::LocalChannal::init(argc, argv, name);
    mutable_communicator_service_ = 
        coin::data::Communicator::service<MutableServerReq, MutableServerAck>("$$/" + node_name() + "/service/mutable_communicator",
            std::bind(&Communicator::mutable_communicator_service_callback_, std::placeholders::_1, std::placeholders::_2));
}

void Communicator::register_mutable_communicator_(const std::string& id, const MutableCommunicator::Ptr& mc)
{
    auto itor = coin::data::local::LocalChannal::communicators().find(id);
    if(itor != coin::data::local::LocalChannal::communicators().end())
    {
        coin::Print::info("redirect [{}] to {}", id, itor->second);
        mc->redirect( itor->second );
    }

    mutable_communicator()->insert_or_assign(id, mc);
}

bool Communicator::mutable_communicator_service_callback_(
        const MutableServerReq& req,
        MutableServerAck& ack
    )
{
    ack.is_ok = true;
    auto id = to_std_string(req.comm_id);
    auto itor = mutable_communicator()->find( id );
    if(itor == mutable_communicator()->end())
    {
        ack.is_ok = false;
        ack.msg = from_std_string("communicator <" + id + "> is not exist.");
        return true;
    }

    if(req.target_name.empty())
    {
        ack.msg = from_std_string("disconnect.");
        ack.is_ok = true;
        itor->second->disconnect();
    }
    else
    {
        ack.msg = from_std_string("redirect to <" + to_std_string(req.target_name) + ">.");
        ack.is_ok = true;
        itor->second->redirect( to_std_string( req.target_name ) );
    }

    return true;
}
} // namespace coin::data
