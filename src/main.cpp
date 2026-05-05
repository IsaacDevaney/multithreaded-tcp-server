#include "logger.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "client_handler.h"
#include "thread_pool.h"
#include "logger.h"

using namespace std;

atomic<bool> server_running(true);
int server_fd_global = -1;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        server_running = false;

        if (server_fd_global != -1) {
            close(server_fd_global);
        }
    }
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt() failed\n";
        close(server_fd);
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        cerr << "bind() failed: " << strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }

    ThreadPool pool(worker_count);

    log_message("KV server listening on port " + to_string(port) +
                " with " + to_string(worker_count) + " worker threads...");

    while (server_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );

        if (client_fd < 0) {
            if (!server_running) {
                break;
            }

            log_message(string("accept() failed: ") + strerror(errno));
            continue;
        }

        log_message("Client accepted");

        pool.enqueue([client_fd] {
            handle_client(client_fd);
        });
    }

    server_running = false;

    if (server_fd_global != -1) {
        close(server_fd_global);
        server_fd_global = -1;
    }

    log_message("Server stopped");
    return 0;
}