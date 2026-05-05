#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <cstring>
#include <iostream>
std::unordered_map<std::string, std::string> store;
int main(int argc, char* argv[]) {
    int port = 8080;

    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket() failed\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind() failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "listen() failed\n";
        close(server_fd);
        return 1;
    }

    std::cout << "KV Server listening on port " << port << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            std::cerr << "accept() failed\n";
            continue;
        }

        std::cout << "Client connected\n";

        const char* message = "Connected to KV server\n";
        send(client_fd, message, std::strlen(message), 0);

        char buffer[1024];

        while (true) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

            if (bytes_read <= 0) {
                break;
            }

            buffer[bytes_read] = '\0';
            std::cout << "Received: " << buffer;

            std::string line(buffer);
            std::istringstream iss(line);

            std::string command;
            std::string key;
            std::string value;

            iss >> command;

            std::string response;

            if (command == "SET") {
                iss >> key;
                std::getline(iss >> std::ws, value);

                if (key.empty() || value.empty()) {
                    response = "ERROR missing key or value\n";
                } else {
                    store[key] = value;
                    response = "OK\n";
                }
            }
            else if (command == "GET") {
                iss >> key;

                auto it = store.find(key);
                if (it == store.end()) {
                    response = "NOT_FOUND\n";
                } else {
                    response = "VALUE " + it->second + "\n";
                }
            }
            else if (command == "DELETE") {
                iss >> key;

                if (store.erase(key) > 0) {
                    response = "DELETED\n";
                } else {
                    response = "NOT_FOUND\n";
                }
            }
            else if (command == "EXISTS") {
                iss >> key;

                response = store.count(key) ? "1\n" : "0\n";
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
        std::cout << "Client disconnected\n";
    }

    close(server_fd);
    return 0;
}