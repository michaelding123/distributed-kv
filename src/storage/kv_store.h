#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class WAL;

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
  // Create store without persistence
  KVStore();

  // Destructor (defined in .cpp because WAL is forward-declared)
  ~KVStore();

  // Create store with WAL persistence at the given path
  // Replays existing WAL entries on construction
  explicit KVStore(const std::string& wal_path);

  // Returns the entry if found, nullopt otherwise
  std::optional<Entry> Get(const std::string& key);

  // Stores value and returns the new version
  // If request_id was seen before for this key, returns cached result
  PutResult Put(const std::string& key, const std::string& value,
                const std::string& request_id);

  // Returns true if key existed and was deleted
  // If request_id was seen before for this key, returns cached result
  DeleteResult Delete(const std::string& key, const std::string& request_id);

  // Compact the WAL by taking a snapshot and resetting the WAL
  // This reduces disk usage and speeds up recovery
  // Returns true on success
  bool Compact();

  // Get the number of keys in the store
  size_t Size() const;

 private:
  // Internal put without WAL logging (used during replay)
  PutResult PutInternal(const std::string& key, const std::string& value,
                        const std::string& request_id);

  // Internal delete without WAL logging (used during replay)
  DeleteResult DeleteInternal(const std::string& key,
                              const std::string& request_id);

  struct KeyState {
    Entry entry;
    std::unordered_map<std::string, uint64_t> put_cache;     // request_id -> version
    std::unordered_map<std::string, bool> delete_cache;      // request_id -> ok
  };

  std::unordered_map<std::string, KeyState> store_;
  mutable std::shared_mutex mu_;
  std::unique_ptr<WAL> wal_;
  std::string wal_path_;
  std::string snapshot_path_;
};
