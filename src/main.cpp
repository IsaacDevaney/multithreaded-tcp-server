#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;

unordered_map<string, string> store;
mutex store_mutex;
mutex cout_mutex;

queue<int> client_queue;
mutex queue_mutex;
condition_variable queue_cv;

atomic<bool> server_running(true);
int server_fd_global = -1;

void log_message(const string& message) {
    lock_guard<mutex> lock(cout_mutex);
    cout << message << endl;
}

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        server_running = false;

        if (server_fd_global != -1) {
            close(server_fd_global);
        }

        queue_cv.notify_all();
    }
}

void handle_client(int client_fd) {
    char buffer[1024];

    while (server_running) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';
        cout << "Received: " << buffer;

        string line(buffer);
        istringstream iss(line);

        string command;
        string key;
        string value;
        string response;

        iss >> command;

        if (command == "SET") {
            iss >> key;
            getline(iss >> ws, value);

            if (key.empty() || value.empty()) {
                response = "ERROR missing key or value\n";
            } else {
                lock_guard<mutex> lock(store_mutex);
                store[key] = value;
                response = "OK\n";
            }
        }
        else if (command == "GET") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                lock_guard<mutex> lock(store_mutex);

                auto it = store.find(key);

                if (it == store.end()) {
                    response = "NOT_FOUND\n";
                } else {
                    response = "VALUE " + it->second + "\n";
                }
            }
        }
        else if (command == "DELETE") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                lock_guard<mutex> lock(store_mutex);

                if (store.erase(key) > 0) {
                    response = "DELETED\n";
                } else {
                    response = "NOT_FOUND\n";
                }
            }
        }
        else if (command == "EXISTS") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                lock_guard<mutex> lock(store_mutex);
                response = store.count(key) ? "1\n" : "0\n";
            }
        }
        else if (command == "QUIT") {
            response = "BYE\n";
            send(client_fd, response.c_str(), response.size(), 0);
            break;
        }
        else {
            response = "ERROR unknown command\n";
        }

        send(client_fd, response.c_str(), response.size(), 0);
    }

    close(client_fd);
    cout << "Client disconnected\n";
}

void worker_loop(int worker_id) {
    cout << "Worker " << worker_id << " started\n";

    while (true) {
        int client_fd;

        {
            unique_lock<mutex> lock(queue_mutex);

            queue_cv.wait(lock, [] {
                return !client_queue.empty() || !server_running;
            });

            if (!server_running && client_queue.empty()) {
                break;
            }

            client_fd = client_queue.front();
            client_queue.pop();
        }

        cout << "Worker " << worker_id << " handling client\n";
        handle_client(client_fd);
    }

    cout << "Worker " << worker_id << " stopped\n";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int port = 8080;
    int worker_count = 4;

    if (argc >= 2) {
        port = stoi(argv[1]);
    }

    if (argc >= 3) {
        worker_count = stoi(argv[2]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_fd_global = server_fd;

    if (server_fd < 0) {
        cerr << "socket() failed\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        cerr << "bind() failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        cerr << "listen() failed\n";
        close(server_fd);
        return 1;
    }

    vector<thread> workers;

    for (int i = 0; i < worker_count; ++i) {
        workers.emplace_back(worker_loop, i + 1);
    }

    cout << "KV server listening on port " << port << " with "
         << worker_count << " worker threads...\n";

    while (server_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (client_fd < 0) {
            if (!server_running) {
                break;
            }

            cerr << "accept() failed\n";
            continue;
        }

        cout << "Client accepted\n";

        {
            lock_guard<mutex> lock(queue_mutex);
            client_queue.push(client_fd);
        }

        queue_cv.notify_one();
    }

    server_running = false;
    queue_cv.notify_all();

    for (thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    close(server_fd);
    cout << "Server stopped\n";

    return 0;
}