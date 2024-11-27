cmake_minimum_required(VERSION 3.16)
project(coin-data)

add_executable(${PROJECT_NAME}-writer writer.cpp)
target_link_libraries(${PROJECT_NAME}-writer
    coin-data
    coin-commons
)

add_executable(${PROJECT_NAME}-reader reader.cpp)
target_link_libraries(${PROJECT_NAME}-reader
    coin-data
    coin-commons
)

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

add_executable(${PROJECT_NAME}-service service.cpp)
target_link_libraries(${PROJECT_NAME}-service
    coin-data
    coin-commons
)

add_executable(${PROJECT_NAME}-client client.cpp)
target_link_libraries(${PROJECT_NAME}-client
    coin-data
    coin-commons
)

add_executable(${PROJECT_NAME}-force-writer force-writer.cpp)
target_link_libraries(${PROJECT_NAME}-force-writer
    coin-data
    coin-commons
)

add_executable(${PROJECT_NAME}-force-reader force-reader.cpp)
target_link_libraries(${PROJECT_NAME}-force-reader
    coin-data
    coin-commons
)

# node test
add_executable(${PROJECT_NAME}-node ${CMAKE_CURRENT_SOURCE_DIR}/node/node.cpp)
target_link_libraries(${PROJECT_NAME}-node
    coin-data
    coin-commons
)
add_executable(${PROJECT_NAME}-node-monitor ${CMAKE_CURRENT_SOURCE_DIR}/node/monitor.cpp)
target_link_libraries(${PROJECT_NAME}-node-monitor
    coin-data
    coin-commons
)
