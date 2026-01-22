#include <gtest/gtest.h>

#include "server/kv_service.h"

class StorageTest : public ::testing::Test {
 protected:
  KVStore store_;
  KVServiceImpl service_{store_};

  // Helper to put a key-value pair
  kv::PutResponse Put(const std::string& key, const std::string& value,
                      const std::string& request_id = "") {
    kv::PutRequest req;
    req.set_key(key);
    req.set_value(value);
    req.set_request_id(request_id);
    kv::PutResponse resp;
    auto status = service_.Put(nullptr, &req, &resp);
    EXPECT_TRUE(status.ok());
    return resp;
  }

  // Helper to get a key
  kv::GetResponse Get(const std::string& key) {
    kv::GetRequest req;
    req.set_key(key);
    kv::GetResponse resp;
    auto status = service_.Get(nullptr, &req, &resp);
    EXPECT_TRUE(status.ok());
    return resp;
  }

  // Helper to delete a key
  kv::DeleteResponse Delete(const std::string& key,
                            const std::string& request_id = "") {
    kv::DeleteRequest req;
    req.set_key(key);
    req.set_request_id(request_id);
    kv::DeleteResponse resp;
    auto status = service_.Delete(nullptr, &req, &resp);
    EXPECT_TRUE(status.ok());
    return resp;
  }
};

TEST_F(StorageTest, PutGetRoundTrip) {
  auto put_resp = Put("foo", "bar");
  EXPECT_EQ(put_resp.version(), 1);

  auto get_resp = Get("foo");
  EXPECT_TRUE(get_resp.found());
  EXPECT_EQ(get_resp.value(), "bar");
  EXPECT_EQ(get_resp.version(), 1);
}

TEST_F(StorageTest, GetNonExistentKey) {
  auto get_resp = Get("nonexistent");
  EXPECT_FALSE(get_resp.found());
}

TEST_F(StorageTest, OverwriteSameKey) {
  Put("key", "value1");
  auto put_resp = Put("key", "value2");
  EXPECT_EQ(put_resp.version(), 2);

  auto get_resp = Get("key");
  EXPECT_TRUE(get_resp.found());
  EXPECT_EQ(get_resp.value(), "value2");
  EXPECT_EQ(get_resp.version(), 2);
}

TEST_F(StorageTest, VersionIncrementsOnEachPut) {
  for (int i = 1; i <= 5; ++i) {
    auto put_resp = Put("counter", "v" + std::to_string(i));
    EXPECT_EQ(put_resp.version(), i);
  }
}

TEST_F(StorageTest, DeleteExistingKey) {
  Put("to_delete", "value");

  auto del_resp = Delete("to_delete");
  EXPECT_TRUE(del_resp.ok());

  auto get_resp = Get("to_delete");
  EXPECT_FALSE(get_resp.found());
}

TEST_F(StorageTest, DeleteNonExistentKey) {
  auto del_resp = Delete("never_existed");
  EXPECT_FALSE(del_resp.ok());
}

TEST_F(StorageTest, PutAfterDelete) {
  Put("reuse", "first");
  Delete("reuse");

  // Version should reset to 1 since Entry is recreated
  auto put_resp = Put("reuse", "second");
  EXPECT_EQ(put_resp.version(), 1);

  auto get_resp = Get("reuse");
  EXPECT_TRUE(get_resp.found());
  EXPECT_EQ(get_resp.value(), "second");
}

TEST_F(StorageTest, MultipleKeys) {
  Put("a", "1");
  Put("b", "2");
  Put("c", "3");

  EXPECT_EQ(Get("a").value(), "1");
  EXPECT_EQ(Get("b").value(), "2");
  EXPECT_EQ(Get("c").value(), "3");

  Delete("b");

  EXPECT_TRUE(Get("a").found());
  EXPECT_FALSE(Get("b").found());
  EXPECT_TRUE(Get("c").found());
}

// Idempotency tests

TEST_F(StorageTest, PutIdempotency) {
  // First put with request_id
  auto resp1 = Put("key", "value", "req-1");
  EXPECT_EQ(resp1.version(), 1);

  // Duplicate put with same request_id returns cached version
  auto resp2 = Put("key", "different_value", "req-1");
  EXPECT_EQ(resp2.version(), 1);

  // Value should be unchanged (original value)
  auto get_resp = Get("key");
  EXPECT_EQ(get_resp.value(), "value");
}

TEST_F(StorageTest, PutDifferentRequestIds) {
  // Different request_ids should apply independently
  auto resp1 = Put("key", "value1", "req-1");
  EXPECT_EQ(resp1.version(), 1);

  auto resp2 = Put("key", "value2", "req-2");
  EXPECT_EQ(resp2.version(), 2);

  auto get_resp = Get("key");
  EXPECT_EQ(get_resp.value(), "value2");
  EXPECT_EQ(get_resp.version(), 2);
}

TEST_F(StorageTest, DeleteIdempotency) {
  Put("key", "value");

  // First delete with request_id
  auto resp1 = Delete("key", "del-1");
  EXPECT_TRUE(resp1.ok());

  // Duplicate delete with same request_id returns cached result
  auto resp2 = Delete("key", "del-1");
  EXPECT_TRUE(resp2.ok());

  // Key should still be deleted
  EXPECT_FALSE(Get("key").found());
}

TEST_F(StorageTest, DeleteNonExistentIdempotency) {
  // Delete non-existent key
  auto resp1 = Delete("missing", "del-1");
  EXPECT_FALSE(resp1.ok());

  // Duplicate returns same cached result
  auto resp2 = Delete("missing", "del-1");
  EXPECT_FALSE(resp2.ok());
}

TEST_F(StorageTest, PutWithoutRequestId) {
  // Puts without request_id are not deduplicated
  auto resp1 = Put("key", "value1");
  EXPECT_EQ(resp1.version(), 1);

  auto resp2 = Put("key", "value2");
  EXPECT_EQ(resp2.version(), 2);

  EXPECT_EQ(Get("key").value(), "value2");
}
