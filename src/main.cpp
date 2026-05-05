#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

using namespace std;

unordered_map<string, string> store;
mutex store_mutex;

void handle_client(int client_fd) {
    char buffer[1024];

    while (true) {
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

int main(int argc, char* argv[]) {
    int port = 8080;

    if (argc >= 2) {
        port = stoi(argv[1]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    if (listen(server_fd, 5) < 0) {
        cerr << "listen() failed\n";
        close(server_fd);
        return 1;
    }

    cout << "KV server listening on port " << port << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            cerr << "accept() failed\n";
            continue;
        }

        cout << "Client connected\n";

        thread client_thread(handle_client, client_fd);
        client_thread.detach();
    }

    close(server_fd);
    return 0;
}