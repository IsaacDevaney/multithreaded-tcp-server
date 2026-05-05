#pragma once

#include <string>

void kv_set(const std::string& key, const std::string& value);
bool kv_get(const std::string& key, std::string& value_out);
bool kv_delete(const std::string& key);
bool kv_exists(const std::string& key);
void kv_clear();