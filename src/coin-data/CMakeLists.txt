CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(coin-data)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/           SRCS)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/local      SRCS)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/local/impl SRCS)

ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    rt
    uuid
    pthread
    fmt::fmt
)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

EXPORT(TARGETS ${PROJECT_NAME} NAMESPACE coin:: FILE ${PROJECT_NAME}.cmake)
if(${COIN_ENABLE_TEST})
    ADD_SUBDIRECTORY(test)
endif()

INSTALL(TARGETS coin-data
    EXPORT CoinTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
