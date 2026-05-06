#include "client_handler.h"
#include "kv_store.h"
#include "logger.h"
#include "metrics.h"

#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <sstream>
#include <string>

using namespace std;

extern atomic<bool> server_running;

namespace {
    string process_command(const string& line, bool& should_close) {
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
            should_close = true;
            response = "BYE\n";
        }
        else if (command.empty()) {
            response = "";
        }
        else {
            response = "ERROR unknown command\n";
        }

        return response;
    }
}

void handle_client(int client_fd) {
    char buffer[1024];
    string pending_data;

    while (server_running) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_read <= 0) {
            break;
        }

        pending_data.append(buffer, bytes_read);

        size_t newline_pos;

        while ((newline_pos = pending_data.find('\n')) != string::npos) {
            string line = pending_data.substr(0, newline_pos);
            pending_data.erase(0, newline_pos + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            log_message("Received command: " + line);

            bool should_close = false;
            string response = process_command(line, should_close);

            if (!response.empty()) {
                record_request();

                ssize_t bytes_sent = send(client_fd, response.c_str(), response.size(), 0);

                if (bytes_sent < 0) {
                    log_message("send() failed while responding to client");
                    break;
                }
            }
            if (should_close) {
                close(client_fd);
                log_message("Client disconnected");
                return;
            }
        }
    }

    close(client_fd);
    log_message("Client disconnected");
}