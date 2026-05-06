cmake_minimum_required(VERSION 3.16)

project(multithreaded_tcp_kv_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kv_server
        src/main.cpp
        src/thread_pool.cpp
        src/kv_store.cpp
        src/client_handler.cpp
        src/logger.cpp
        src/metrics.cpp
)

target_include_directories(kv_server PRIVATE include)
target_compile_options(kv_server PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kv_server PRIVATE pthread)

include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(kv_store_tests
        tests/kv_store_tests.cpp
        src/kv_store.cpp
)

target_include_directories(kv_store_tests PRIVATE include)

target_link_libraries(kv_store_tests
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(kv_store_tests)
# add_executable(protocol_tests
#     tests/protocol_tests.cpp
#     src/protocol.cpp
# )
#
# target_include_directories(protocol_tests PRIVATE include)
#
# target_link_libraries(protocol_tests
#     GTest::gtest_main
# )
#
# gtest_discover_tests(protocol_tests)
add_executable(load_client
        benchmark/load_client.cpp
)

target_compile_options(load_client PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(load_client PRIVATE pthread)

