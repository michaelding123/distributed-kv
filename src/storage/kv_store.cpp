#include "kv_store.h"

std::optional<Entry> KVStore::Get(const std::string& key) {
  std::shared_lock lock(mu_);
  auto it = store_.find(key);
  if (it == store_.end() || it->second.entry.version == 0) {
    return std::nullopt;
  }
  return it->second.entry;
}

PutResult KVStore::Put(const std::string& key, const std::string& value,
                       const std::string& request_id) {
  std::unique_lock lock(mu_);
  auto& state = store_[key];

  // Check for duplicate request
  if (!request_id.empty()) {
    auto it = state.put_cache.find(request_id);
    if (it != state.put_cache.end()) {
      return {it->second, true};
    }
  }

  // Apply the write
  state.entry.value = value;
  state.entry.version++;

  // Cache the result for idempotency
  if (!request_id.empty()) {
    state.put_cache[request_id] = state.entry.version;
  }

  return {state.entry.version, false};
}

DeleteResult KVStore::Delete(const std::string& key,
                             const std::string& request_id) {
  std::unique_lock lock(mu_);
  auto& state = store_[key];

  // Check for duplicate request
  if (!request_id.empty()) {
    auto cache_it = state.delete_cache.find(request_id);
    if (cache_it != state.delete_cache.end()) {
      return {cache_it->second, true};
    }
  }

  // Apply the delete (version > 0 means key exists)
  bool ok = state.entry.version > 0;
  if (ok) {
    state.entry = Entry{};  // Reset to default (version = 0)
  }

  // Cache the result for idempotency
  if (!request_id.empty()) {
    state.delete_cache[request_id] = ok;
  }

  return {ok, false};
}
