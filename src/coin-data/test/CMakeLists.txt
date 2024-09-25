cmake_minimum_required(VERSION 3.16)
project(coin-data)

add_executable(${PROJECT_NAME}-publisher publisher.cpp)
target_link_libraries(${PROJECT_NAME}-publisher
    coin-data
    coin-commons
)

add_executable(${PROJECT_NAME}-subscriber subscriber.cpp)
target_link_libraries(${PROJECT_NAME}-subscriber
    coin-data
    coin-commons
)
