# Multithreaded TCP Key-Value Server (C++)

## Features
- TCP server using POSIX sockets
- Command protocol: SET, GET, DELETE, EXISTS, QUIT
- In-memory key-value store (unordered_map)
- Line-based protocol parsing

## Build
cmake -S . -B build
cmake --build build

## Run
./build/kv_server 8080

## Test
nc localhost 8080