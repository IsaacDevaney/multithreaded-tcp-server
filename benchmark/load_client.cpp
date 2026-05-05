#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace std;

struct ClientMetrics {
    long long requests_completed = 0;
    vector<double> latencies_ms;
};

int connect_to_server(const string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

bool send_all(int sock, const string& message) {
    size_t total_sent = 0;

    while (total_sent < message.size()) {
        ssize_t sent = send(sock, message.c_str() + total_sent, message.size() - total_sent, 0);

        if (sent <= 0) {
            return false;
        }

        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

bool recv_response(int sock, string& response) {
    char buffer[1024];

    ssize_t bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        return false;
    }

    buffer[bytes_read] = '\0';
    response = buffer;
    return true;
}

void client_worker(
    int client_id,
    const string& host,
    int port,
    int requests_per_client,
    ClientMetrics& metrics
) {
    int sock = connect_to_server(host, port);

    if (sock < 0) {
        cerr << "Client " << client_id << " failed to connect\n";
        return;
    }

    metrics.latencies_ms.reserve(requests_per_client);

    for (int i = 0; i < requests_per_client; ++i) {
        string key = "client" + to_string(client_id) + "_key" + to_string(i);
        string value = "value" + to_string(i);

        string command;

        switch (i % 4) {
            case 0:
                command = "SET " + key + " " + value + "\n";
                break;
            case 1:
                command = "GET " + key + "\n";
                break;
            case 2:
                command = "EXISTS " + key + "\n";
                break;
            default:
                command = "DELETE " + key + "\n";
                break;
        }

        auto start = chrono::high_resolution_clock::now();

        if (!send_all(sock, command)) {
            break;
        }

        string response;
        if (!recv_response(sock, response)) {
            break;
        }

        auto end = chrono::high_resolution_clock::now();

        double latency_ms = chrono::duration<double, milli>(end - start).count();
        metrics.latencies_ms.push_back(latency_ms);
        metrics.requests_completed++;
    }

    send_all(sock, "QUIT\n");

    string response;
    recv_response(sock, response);

    close(sock);
}

int main(int argc, char* argv[]) {
    string host = "127.0.0.1";
    int port = 8080;
    int client_count = 100;
    int requests_per_client = 100;

    if (argc >= 2) {
        client_count = stoi(argv[1]);
    }

    if (argc >= 3) {
        requests_per_client = stoi(argv[2]);
    }

    if (argc >= 4) {
        port = stoi(argv[3]);
    }

    vector<thread> threads;
    vector<ClientMetrics> all_metrics(client_count);

    cout << "Starting load test...\n";
    cout << "Clients: " << client_count << '\n';
    cout << "Requests per client: " << requests_per_client << '\n';
    cout << "Target: " << host << ":" << port << "\n\n";

    auto test_start = chrono::high_resolution_clock::now();

    for (int i = 0; i < client_count; ++i) {
        threads.emplace_back(
            client_worker,
            i + 1,
            host,
            port,
            requests_per_client,
            ref(all_metrics[i])
        );
    }

    for (thread& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto test_end = chrono::high_resolution_clock::now();

    double elapsed_seconds = chrono::duration<double>(test_end - test_start).count();

    long long total_requests = 0;
    vector<double> all_latencies;

    for (const auto& metrics : all_metrics) {
        total_requests += metrics.requests_completed;
        all_latencies.insert(
            all_latencies.end(),
            metrics.latencies_ms.begin(),
            metrics.latencies_ms.end()
        );
    }

    double requests_per_second = total_requests / elapsed_seconds;

    double average_latency = 0.0;
    double min_latency = 0.0;
    double max_latency = 0.0;

    if (!all_latencies.empty()) {
        average_latency = accumulate(all_latencies.begin(), all_latencies.end(), 0.0)
                        / all_latencies.size();

        min_latency = *min_element(all_latencies.begin(), all_latencies.end());
        max_latency = *max_element(all_latencies.begin(), all_latencies.end());
    }

    cout << "Load test complete\n";
    cout << "Total requests completed: " << total_requests << '\n';
    cout << "Elapsed time: " << elapsed_seconds << " seconds\n";
    cout << "Throughput: " << requests_per_second << " requests/sec\n";
    cout << "Average latency: " << average_latency << " ms\n";
    cout << "Minimum latency: " << min_latency << " ms\n";
    cout << "Maximum latency: " << max_latency << " ms\n";

    ofstream results("docs/test-results/load-test-results.txt");

    if (results) {
        results << "Load Test Results\n";
        results << "=================\n";
        results << "Clients: " << client_count << '\n';
        results << "Requests per client: " << requests_per_client << '\n';
        results << "Total requests completed: " << total_requests << '\n';
        results << "Elapsed time: " << elapsed_seconds << " seconds\n";
        results << "Throughput: " << requests_per_second << " requests/sec\n";
        results << "Average latency: " << average_latency << " ms\n";
        results << "Minimum latency: " << min_latency << " ms\n";
        results << "Maximum latency: " << max_latency << " ms\n";
    }

    return 0;
}