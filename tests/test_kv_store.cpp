#include "kv_store.h"

#include <cassert>
#include <iostream>
#include <string>

int tests_run = 0;

void report_pass(const std::string& test_name) {
    ++tests_run;
    std::cout << "[PASS] " << test_name << '\n';
}

void test_set_and_get() {
    kv_clear();

    kv_set("name", "Isaac");

    std::string value;
    bool found = kv_get("name", value);

    assert(found);
    assert(value == "Isaac");

    report_pass("SET stores a key/value pair and GET retrieves it");
}

void test_get_missing_key() {
    kv_clear();

    std::string value;
    bool found = kv_get("missing", value);

    assert(!found);

    report_pass("GET returns false for a missing key");
}

void test_delete_existing_key() {
    kv_clear();

    kv_set("name", "Isaac");

    bool deleted = kv_delete("name");
    bool exists = kv_exists("name");

    assert(deleted);
    assert(!exists);

    report_pass("DELETE removes an existing key");
}

void test_delete_missing_key() {
    kv_clear();

    bool deleted = kv_delete("missing");

    assert(!deleted);

    report_pass("DELETE returns false for a missing key");
}

void test_exists() {
    kv_clear();

    kv_set("x", "100");

    assert(kv_exists("x"));
    assert(!kv_exists("y"));

    report_pass("EXISTS reports present and absent keys correctly");
}

int main() {
    std::cout << "Running kv_store unit tests...\n\n";

    test_set_and_get();
    test_get_missing_key();
    test_delete_existing_key();
    test_delete_missing_key();
    test_exists();

    std::cout << "\nSummary: " << tests_run << " / " << tests_run
              << " kv_store tests passed\n";

    return 0;
}