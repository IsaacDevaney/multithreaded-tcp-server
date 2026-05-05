#include "kv_store.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string> store;
    std::mutex store_mutex;
}

void kv_set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(store_mutex);
    store[key] = value;
}

bool kv_get(const std::string& key, std::string& value_out) {
    std::lock_guard<std::mutex> lock(store_mutex);

    auto it = store.find(key);
    if (it == store.end()) {
        return false;
    }

    value_out = it->second;
    return true;
}

bool kv_delete(const std::string& key) {
    std::lock_guard<std::mutex> lock(store_mutex);
    return store.erase(key) > 0;
}

bool kv_exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(store_mutex);
    return store.count(key) > 0;
}

void kv_clear() {
    std::lock_guard<std::mutex> lock(store_mutex);
    store.clear();
}