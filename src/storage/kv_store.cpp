#include "kv_store.h"

std::optional<Entry> KVStore::Get(const std::string& key) {
  std::shared_lock lock(mu_);
  auto it = store_.find(key);
  if (it == store_.end()) {
    return std::nullopt;
  }
  return it->second;
}

uint64_t KVStore::Put(const std::string& key, const std::string& value) {
  std::unique_lock lock(mu_);
  auto& e = store_[key];
  e.value = value;
  e.version++;
  return e.version;
}

bool KVStore::Delete(const std::string& key) {
  std::unique_lock lock(mu_);
  return store_.erase(key) > 0;
}
