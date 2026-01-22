#pragma once

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

struct Entry {
  std::string value;
  uint64_t version = 0;
};

struct PutResult {
  uint64_t version;
  bool was_duplicate;
};

struct DeleteResult {
  bool ok;
  bool was_duplicate;
};

class KVStore {
 public:
  // Returns the entry if found, nullopt otherwise
  std::optional<Entry> Get(const std::string& key);

  // Stores value and returns the new version
  // If request_id was seen before for this key, returns cached result
  PutResult Put(const std::string& key, const std::string& value,
                const std::string& request_id);

  // Returns true if key existed and was deleted
  // If request_id was seen before for this key, returns cached result
  DeleteResult Delete(const std::string& key, const std::string& request_id);

 private:
  struct KeyState {
    Entry entry;
    std::unordered_map<std::string, uint64_t> put_cache;     // request_id -> version
    std::unordered_map<std::string, bool> delete_cache;      // request_id -> ok
  };

  std::unordered_map<std::string, KeyState> store_;
  mutable std::shared_mutex mu_;
};
