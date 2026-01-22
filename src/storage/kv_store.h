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

class KVStore {
 public:
  // Returns the entry if found, nullopt otherwise
  std::optional<Entry> Get(const std::string& key);

  // Stores value and returns the new version
  uint64_t Put(const std::string& key, const std::string& value);

  // Returns true if key existed and was deleted
  bool Delete(const std::string& key);

 private:
  std::unordered_map<std::string, Entry> store_;
  mutable std::shared_mutex mu_;
};
