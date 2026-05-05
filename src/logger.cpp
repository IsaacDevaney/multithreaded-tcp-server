#include "logger.h"

#include <iostream>
#include <mutex>

namespace {
    std::mutex cout_mutex;
}

void log_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << message << std::endl;
}