#include "kv_store.h"

#include <iostream>
#include <mutex>

#include "snapshot.h"
#include "wal.h"

KVStore::KVStore() = default;
KVStore::~KVStore() = default;

KVStore::KVStore(const std::string& wal_path)
    : wal_path_(wal_path), snapshot_path_(wal_path + ".snapshot") {
  // First, try to load snapshot
  std::unordered_map<std::string, Entry> snapshot_data;
  if (Snapshot::Load(snapshot_path_, snapshot_data)) {
    std::cout << "Loaded snapshot with " << snapshot_data.size() << " keys.\n";
    for (auto& [key, entry] : snapshot_data) {
      store_[key].entry = std::move(entry);
    }
  }

  // Then replay WAL entries (applied on top of snapshot)
  WAL temp_wal(wal_path);
  auto entries = temp_wal.ReadAll();

  if (!entries.empty()) {
    std::cout << "Replaying " << entries.size() << " WAL entries...\n";

    for (const auto& entry : entries) {
      if (entry.type == WALEntryType::PUT) {
        PutInternal(entry.key, entry.value, entry.request_id);
      } else if (entry.type == WALEntryType::DELETE) {
        DeleteInternal(entry.key, entry.request_id);
      }
    }
  }

  // Now create the WAL for new writes (appends to existing file)
  wal_ = std::make_unique<WAL>(wal_path);

  std::cout << "Recovery complete. Store has " << store_.size() << " keys.\n";
}

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
  // Log to WAL before applying (write-ahead)
  if (wal_) {
    wal_->LogPut(key, value, request_id);
  }
  return PutInternal(key, value, request_id);
}

PutResult KVStore::PutInternal(const std::string& key, const std::string& value,
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
  // Log to WAL before applying (write-ahead)
  if (wal_) {
    wal_->LogDelete(key, request_id);
  }
  return DeleteInternal(key, request_id);
}

DeleteResult KVStore::DeleteInternal(const std::string& key,
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

bool KVStore::Compact() {
  if (!wal_ || snapshot_path_.empty()) {
    return false;  // No persistence configured
  }

  std::unique_lock lock(mu_);

  // Build snapshot data from current state
  std::unordered_map<std::string, Entry> snapshot_data;
  for (const auto& [key, state] : store_) {
    if (state.entry.version > 0) {
      snapshot_data[key] = state.entry;
    }
  }

  // Save snapshot
  if (!Snapshot::Save(snapshot_path_, snapshot_data)) {
    std::cerr << "Compact: Failed to save snapshot\n";
    return false;
  }

  // Reset WAL (truncate)
  wal_->Reset();

  std::cout << "Compacted: snapshot has " << snapshot_data.size()
            << " keys, WAL reset.\n";

  return true;
}

size_t KVStore::Size() const {
  std::shared_lock lock(mu_);
  size_t count = 0;
  for (const auto& [key, state] : store_) {
    if (state.entry.version > 0) {
      count++;
    }
  }
  return count;
}
