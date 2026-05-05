cmake_minimum_required(VERSION 3.16)
project(multithreaded_tcp_kv_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kv_server
        src/main.cpp
        src/server.cpp
        src/thread_pool.cpp
        src/kv_store.cpp
        src/protocol.cpp
        include/server.hpp
        include/server.hpp
        src/main.cpp
)

target_include_directories(kv_server PRIVATE include)
target_compile_options(kv_server PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kv_server PRIVATE pthread)