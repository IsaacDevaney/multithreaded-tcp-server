#include "metrics.h"
#include "logger.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <string>

namespace {
    std::atomic<long long> total_requests{0};
    std::atomic<bool> metrics_running{false};
    std::thread metrics_thread;
}

void record_request() {
    total_requests.fetch_add(1, std::memory_order_relaxed);
}

void start_metrics_logger() {
    metrics_running = true;

    metrics_thread = std::thread([] {
        using namespace std::chrono;

        const auto start_time = steady_clock::now();
        long long last_count = 0;

        while (metrics_running) {
            std::this_thread::sleep_for(seconds(5));

            long long current_count = total_requests.load(std::memory_order_relaxed);
            long long interval_requests = current_count - last_count;
            last_count = current_count;

            auto now = steady_clock::now();
            double uptime_seconds = duration<double>(now - start_time).count();

            double average_rps = uptime_seconds > 0.0
                ? current_count / uptime_seconds
                : 0.0;

            double interval_rps = interval_requests / 5.0;

            log_message(
                "[metrics] total_requests=" + std::to_string(current_count) +
                " uptime_sec=" + std::to_string(static_cast<int>(uptime_seconds)) +
                " avg_rps=" + std::to_string(average_rps) +
                " interval_rps=" + std::to_string(interval_rps)
            );
        }
    });
}

void stop_metrics_logger() {
    metrics_running = false;

    if (metrics_thread.joinable()) {
        metrics_thread.join();
    }
}