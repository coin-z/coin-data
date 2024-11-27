/**
 * @file coin-data.cpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <coin-data.hpp>

// namespace coin::data::__inner
// {
// void init(int argc, char *argv[])
// {
//     coin::Print::debug("coin data initialize {}", argv[0]);
//     coin::data::Communicator::init(argc, argv);
// }
// void spin_once()
// {
//     coin::data::Communicator::spin_once();
// }
// } // namespace coin::data

// extern "C" {
// __attribute__((constructor)) void coin_data_initialize_()
// {
//     // coin::__inner::register_init(&coin::data::__inner::init);
//     // coin::__inner::register_spin_once(&coin::data::__inner::spin_once);
// }
// } // extern "C"

int coin::data::init(int argc, char *argv[], const std::string& name)
{
    coin::Print::debug("pid of this process: {}", getpid());
    coin::data::Communicator::init(argc, argv, name);
    return 0;
}
