#include "kv_store.h"

#include <gtest/gtest.h>
#include <string>

TEST(KvStoreTest, SetAndGetStoresValue) {
    kv_clear();

    kv_set("name", "Isaac");

    std::string value;
    bool found = kv_get("name", value);

    EXPECT_TRUE(found);
    EXPECT_EQ(value, "Isaac");
}

TEST(KvStoreTest, GetMissingKeyReturnsFalse) {
    kv_clear();

    std::string value;
    bool found = kv_get("missing", value);

    EXPECT_FALSE(found);
}

TEST(KvStoreTest, DeleteExistingKeyRemovesValue) {
    kv_clear();

    kv_set("name", "Isaac");

    bool deleted = kv_delete("name");

    EXPECT_TRUE(deleted);
    EXPECT_FALSE(kv_exists("name"));
}

TEST(KvStoreTest, DeleteMissingKeyReturnsFalse) {
    kv_clear();

    bool deleted = kv_delete("missing");

    EXPECT_FALSE(deleted);
}

TEST(KvStoreTest, ExistsReportsPresentAndAbsentKeys) {
    kv_clear();

    kv_set("x", "100");

    EXPECT_TRUE(kv_exists("x"));
    EXPECT_FALSE(kv_exists("y"));
}

TEST(KvStoreTest, OverwriteExistingKeyUpdatesValue) {
    kv_clear();

    kv_set("mode", "old");
    kv_set("mode", "new");

    std::string value;
    bool found = kv_get("mode", value);

    EXPECT_TRUE(found);
    EXPECT_EQ(value, "new");
}
