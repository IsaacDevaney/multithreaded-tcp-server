#include "client_handler.h"
#include "logger.h"
#include "kv_store.h"

#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace std;


extern atomic<bool> server_running;


void handle_client(int client_fd) {
    char buffer[1024];

    while (server_running) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';
        log_message(string("Received: ") + buffer);

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
                kv_set(key, value);
                response = "OK\n";
            }
        }
        else if (command == "GET") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                string stored_value;

                if (kv_get(key, stored_value)) {
                    response = "VALUE " + stored_value + "\n";
                } else {
                    response = "NOT_FOUND\n";
                }
            }
        }
        else if (command == "DELETE") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                response = kv_delete(key) ? "DELETED\n" : "NOT_FOUND\n";
            }
        }
        else if (command == "EXISTS") {
            iss >> key;

            if (key.empty()) {
                response = "ERROR missing key\n";
            } else {
                response = kv_exists(key) ? "1\n" : "0\n";
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
    log_message("Client disconnected");
}