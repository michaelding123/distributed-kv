#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

// Write-Ahead Log entry types
enum class WALEntryType : uint8_t {
  PUT = 1,
  DELETE = 2,
};

struct WALEntry {
  WALEntryType type;
  std::string key;
  std::string value;      // Only for PUT
  std::string request_id;
};

class WAL {
 public:
  explicit WAL(const std::string& path);
  ~WAL();

  // Append a PUT entry to the log (called before applying to store)
  void LogPut(const std::string& key, const std::string& value,
              const std::string& request_id);

  // Append a DELETE entry to the log (called before applying to store)
  void LogDelete(const std::string& key, const std::string& request_id);

  // Read all entries from the log (for replay on startup)
  std::vector<WALEntry> ReadAll();

  // Sync the log to disk
  void Sync();

  // Reset (truncate) the WAL - used after snapshotting
  void Reset();

 private:
  void WriteEntry(const WALEntry& entry);
  WALEntry ReadEntry(std::ifstream& in);

  std::string path_;
  std::ofstream out_;
  std::mutex mu_;
};
