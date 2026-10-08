cmake_minimum_required(VERSION 3.10)
project(MiniRedis VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common sources
set(COMMON_SOURCES
    src/core/storage_engine.cpp
    src/protocol/resp_parser.cpp
    src/persistence/wal.cpp
)

# MiniRedis Server Executable
add_executable(mini_redis
    src/main.cpp
    src/network/server.cpp
    ${COMMON_SOURCES}
)

# MiniRedis Automated Test Suite
add_executable(test_suite
    tests/test_engine.cpp
    ${COMMON_SOURCES}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(mini_redis PRIVATE ws2_32)
    target_link_libraries(test_suite PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(mini_redis PRIVATE Threads::Threads)
    target_link_libraries(test_suite PRIVATE Threads::Threads)
endif()

enable_testing()
add_test(NAME EngineTestSuite COMMAND test_suite)
