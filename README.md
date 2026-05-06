# Multithreaded TCP Key-Value Server (C++)

---
## Overview 
This project
implements a Linux TCP key-value server backed by an in-memory hash map. The server uses BSD socket APIs (`socket()`, `bind()`, `listen()`, and `accept()`) to accept TCP client connections, then dispatches accepted sockets to a fixed-size worker thread pool. The main thread accepts connections and enqueues client sockets, while worker threads block on a `std::condition_variable` when no work is available. This design avoids repeatedly creating a new thread for every client connection and bounds scheduler overhead under concurrent load.

The server supports a simple line-based protocol with `SET`, `GET`, `EXISTS`, `DELETE`, and `QUIT` commands. In a local benchmark with 500 concurrent clients sending 100 commands each, the server processed 50,000 total requests in 3.55 seconds, sustaining approximately 14,065 requests/sec with 2.3 ms average latency. The maximum latency spike indicates expected tail-latency pressure under high local concurrency, likely caused by OS scheduling, socket buffering, synchronized logging, and shared-store lock contention. This benchmark demonstrates functional concurrency and measurable throughput rather than production-grade optimization. It was encouraging to see ThreadSanitizer report no runtime concurrency bugs even under heavy client load. The clean run provides additional confidence in the mutex-protected key-value store and the thread-pool synchronization strategy.

---
## Supported Commands
The server uses a simple line-based text protocol. Each client command is sent as a newline-terminated string (`\n`), and the server replies with a plain-text response.
- `SET <key> <value>`  
  Stores `value` under `key`.  
  Response: `OK`

- `GET <key>`  
  Retrieves the value stored under `key`.  
  Response: `VALUE <value>` if found, otherwise `NOT_FOUND`

- `DELETE <key>`  
  Removes `key` from the store.  
  Response: `DELETED` if removed, otherwise `NOT_FOUND`

- `EXISTS <key>`  
  Checks whether `key` exists in the store.  
  Response: `1` if present, otherwise `0`

- `QUIT`  
  Closes the client connection.  
  Response: `BYE`

## Building

This project is written in C++17 and uses CMake as the build system. It was developed and tested on Linux/WSL using `g++`, POSIX sockets, and pthread-backed C++ threads.

### Requirements

- Linux or WSL
- CMake 3.16+
- A C++17-compatible compiler such as `g++` or `clang++`
- `pthread` support
- `nc` / netcat for manual integration testing

### Standard Build

From the project root:

```bash
cmake -S . -B build
cmake --build build
```
To create the main server executable:
```bash
./build/kv_server
```
Depending on your CMake configuration, it may also build additional benchmark executables such as:
```bash
./build/kv_store_tests
./build/load_client
```
The port and worker count may also be specified manually with:
```bash
./build/kv_server <port> <worker_count>
```
For example, the following listens for TCP clients on port 8080 and creates 4 worker threads in the thread pool.
```bash
./build/kv_server 8080 4
```
After running it, you should see:
```bash
KV server listening on port 8080 with 4 worker threads...
```
Then, from another terminal, you can connect with 
```bash
nc localhost 8080
```
Once connection has been established you can send commands and the server should respond as follows:
```bash
$ nc localhost 8080
SET name Alice
OK
GET name
VALUE Alice
EXISTS name
1
DELETE name
DELETED
GET name
NOT_FOUND
QUIT
BYE
```
Here, the server replies as expected to each command. The sample above shows how the client’s
commands (
GET , etc.) and the server’s responses (
OK ,
VALUE ,
DELETED , etc.) appear. You can
run multiple clients concurrently; the server will handle them in parallel via the thread pool.
---

## Testing
#### Unit Tests(GoogleTest): I wrote gtest unit tests for the key-value logic(inserting, deleting, and retrieving). All unit tests passed without errors, as shown below. The GoogleTest framework is thread safe on pthread platforms and widely used. The following image shows the output for kv_store unit tests. All test cases have passed successfully.
![kv_store_GoogleTest1.png](docs/test-results/Benchmark%20Test%20Pictures/kv_store_GoogleTest1.png)

#### ThreadSanitizer(TSAN): 
TSAN is designed to catch concurrency bugs and find data races at runtime. These tests demonstrate the mutexes ability to effectively prevent races. The screenshots below show the TSAN summary for 2 heavy load tests, one with 100 concurrent clients and the other with 500. Despite TSAN's claim on their website that they "will find race conditions you didn't even know were there", **no errors** were reported.
![Tsan100.png](docs/test-results/Benchmark%20Test%20Pictures/Tsan100.png)
![Tsan500.png](docs/test-results/Benchmark%20Test%20Pictures/Tsan500.png)

#### Integration Test:
Integration tests were also performed using `nc`. A shell script sends a predefined sequence of commands in rapid succession to the server and compares the output to the expected results. Throughout testing, `nc` yielded a passing result, verifying the network and command protocols work together correctly. Below is a figure showing the test run via `nc`. The script sends commands such as SET, GET, DELETE, etc, and the output matches the expected results on the integration test.

![Intigration-test.png](docs/test-results/Benchmark%20Test%20Pictures/Intigration-test.png)
![manual-nc-intigration-test.png](docs/test-results/Benchmark%20Test%20Pictures/manual-nc-intigration-test.png)

#### Throughput Test
The figure below shows the results from a multithreaded load test, along with the exact command used to generate the output. `tee` is used to write the benchmark results to a text file while still displaying them in the terminal.
![multithreadedloadtest.png](docs/test-results/Benchmark%20Test%20Pictures/multithreadedloadtest.png)

The second screenshot shows the same 500-client load test run without piping the output through `tee` to save it to a text file. This removes a small amount of shell/file-output overhead from the benchmark command. Interestingly, this run produced slightly lower throughput than the first run, but better average and maximum latency, which suggests normal run-to-run variance from OS scheduling, socket buffering, and thread contention under high local concurrency.
![server-side-request-metrics.png](docs/test-results/Benchmark%20Test%20Pictures/server-side-request-metrics.png)

### Logging and Metrics
The server logs key events to stdout in a thread-safe manner. For example, it prints messages when it starts up,when it accepts a new connection from a client IP/port, receives a command, and when a client disconnects. Periodic performance logging was also implemented: every 5 seconds the server logs total requests, uptime, and average requests/sec. These logs and their metrics summary are written to the docs/test-results/Text Summaries during testing.
For example, threadsanitizer-run.txt and integration-summary.txt capture the data shown above. This continuous logging makes debugging and performance analysis much easier.

## Thread Safety and Concurrency 
Proper locking is critical in a multithreaded server. The key-value store is backed by a shared `std::unordered_map`, so each `SET`, `GET`, `DELETE`, and `EXISTS` operation acquires a mutex before accessing the map. This prevents multiple worker threads from reading and writing the store simultaneously, avoiding data races and keeping the in-memory state consistent.

Because `std::unordered_map` provides average O(1) lookup, insertion, and deletion, each operation spends relatively little time holding the store mutex. Worker threads also use a condition variable to sleep when no client work is available, avoiding busy-waiting.

Thread safety was evaluated using ThreadSanitizer under concurrent client load. The sanitized runs reported no data races, which provides additional confidence in the mutex-protected store and thread-pool synchronization strategy.

This project demonstrates how to build, run, test, and benchmark a multithreaded TCP server using unit tests, integration tests, load testing, runtime metrics, and thread-safety verification. Future improvements could focus on reducing tail-latency pressure under high concurrency by tuning thread-pool size, reducing per-command logging during benchmarks, experimenting with socket options, and reducing shared-store lock contention.
