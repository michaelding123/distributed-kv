#include <gtest/gtest.h>

#include "kv_service.h"

class StorageTest : public ::testing::Test {
 protected:
  KVServiceImpl service_;

  // Helper to put a key-value pair
  kv::PutResponse Put(const std::string& key, const std::string& value) {
    kv::PutRequest req;
    req.set_key(key);
    req.set_value(value);
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
  kv::DeleteResponse Delete(const std::string& key) {
    kv::DeleteRequest req;
    req.set_key(key);
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
