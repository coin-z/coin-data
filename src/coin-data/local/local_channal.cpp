/**
 * @file local_channal.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <regex>
#include <signal.h>
#include <uuid/uuid.h>

#include <coin-commons/utils/utils.hpp>
#include <coin-commons/utils/crc32.hpp>
#include <coin-commons/utils/datetime.hpp>
#include <coin-data/local/local_channal.hpp>

static void sys_exit(int s)
{
    exit(s);
}

namespace coin::data::local
{
LocalChannal::LocalChannal()
{

}

LocalChannal::~LocalChannal()
{ }
LocalChannal &LocalChannal::instance()
{
    static LocalChannal ins;
    return ins;
}

void LocalChannal::init(int argc, char* argv[], const std::string& name)
{
    // calculate self name
    // 1. read self name from env: COIN_NODE_NAME
    // 2. genrate from uuid

    if(not name.empty())
    {
        instance().self_name_ = name;
    }
    else
    {
        const char coin_node_name_key[] = "COIN_NODE_NAME";
        char* name_p = getenv(coin_node_name_key);
        if(name_p)
        {
            instance().self_name_ = std::string(name_p);
        }
        else
        {
            uuid_t id;
            uuid_generate(id);
            char id_str[33];
            for(int i = 0; i < 16; i++)
            {
                std::sprintf(&id_str[i * 2], "%02X", id[i]);
            }
            id_str[32] = '\0';
            instance().self_name_ = std::string(id_str);
        }        
    }

    coin::Print::info("create local channal: {}", instance().self_name_);

    ShmObjManager::instance().init(instance().self_name_);
}

const std::map<std::string, std::string>& LocalChannal::communicators()
{
    auto load_communicators_from_env = []()->std::map<std::string, std::string> {
        std::map<std::string, std::string> node_communicators;
        const char coin_node_communications_key[] = "COIN_NODE_COMMUNICATIONS";
        char* communications_str = getenv(coin_node_communications_key);
        if(communications_str)
        {
            auto str = std::string(communications_str);
            if(str.empty())
            {
                return node_communicators;
            }
            std::regex re("\\s*;+\\s*");
            std::sregex_token_iterator it(str.begin(), str.end(), re, -1);
            std::sregex_token_iterator end;
            while(it != end)
            {
                std::string s = it->str();
                std::regex re_blank("\\s*:+\\s*");
                std::sregex_token_iterator it_blank(s.begin(), s.end(), re_blank, -1);
                std::sregex_token_iterator end_blank;

                const std::string& key = it_blank->str();
                const std::string& value = (++it_blank)->str();
                node_communicators.insert_or_assign(key, value);

                it++;
            }
        }
        return node_communicators;
    };
    static std::map<std::string, std::string> comms = load_communicators_from_env();
    return comms;
}
} // namespace coin::data
