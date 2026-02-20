#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct Entry;

// Snapshot file format:
//   [4 bytes] magic number (0x4B56534E = "KVSN")
//   [4 bytes] number of entries
//   For each entry:
//     [4 bytes] key length
//     [N bytes] key
//     [4 bytes] value length
//     [N bytes] value
//     [8 bytes] version

class Snapshot {
 public:
  static constexpr uint32_t MAGIC = 0x4B56534E;  // "KVSN"

  // Save a snapshot of key-value pairs to a file
  // Only saves entries with version > 0 (non-deleted keys)
  static bool Save(const std::string& path,
                   const std::unordered_map<std::string, Entry>& data);

  // Load a snapshot from a file
  // Returns true if successful, false if file doesn't exist or is invalid
  static bool Load(const std::string& path,
                   std::unordered_map<std::string, Entry>& data);

  // Check if a snapshot file exists
  static bool Exists(const std::string& path);
};
