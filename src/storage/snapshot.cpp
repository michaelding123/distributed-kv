#include "snapshot.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "kv_store.h"  // For Entry struct

bool Snapshot::Save(const std::string& path,
                    const std::unordered_map<std::string, Entry>& data) {
  // Write to temp file first, then rename (atomic)
  std::string temp_path = path + ".tmp";

  std::ofstream out(temp_path, std::ios::binary);
  if (!out) {
    std::cerr << "Snapshot: Failed to open temp file for writing\n";
    return false;
  }

  // Magic number
  uint32_t magic = MAGIC;
  out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

  // Count non-deleted entries
  uint32_t count = 0;
  for (const auto& [key, entry] : data) {
    if (entry.version > 0) {
      count++;
    }
  }
  out.write(reinterpret_cast<const char*>(&count), sizeof(count));

  // Write each entry
  for (const auto& [key, entry] : data) {
    if (entry.version == 0) continue;  // Skip deleted entries

    // Key
    uint32_t key_len = static_cast<uint32_t>(key.size());
    out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    out.write(key.data(), key.size());

    // Value
    uint32_t val_len = static_cast<uint32_t>(entry.value.size());
    out.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
    out.write(entry.value.data(), entry.value.size());

    // Version
    out.write(reinterpret_cast<const char*>(&entry.version), sizeof(entry.version));
  }

  out.close();

  // Atomic rename
  try {
    std::filesystem::rename(temp_path, path);
  } catch (const std::exception& e) {
    std::cerr << "Snapshot: Failed to rename temp file: " << e.what() << "\n";
    return false;
  }

  return true;
}

bool Snapshot::Load(const std::string& path,
                    std::unordered_map<std::string, Entry>& data) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;  // File doesn't exist
  }

  // Magic number
  uint32_t magic;
  if (!in.read(reinterpret_cast<char*>(&magic), sizeof(magic))) {
    std::cerr << "Snapshot: Failed to read magic number\n";
    return false;
  }
  if (magic != MAGIC) {
    std::cerr << "Snapshot: Invalid magic number\n";
    return false;
  }

  // Entry count
  uint32_t count;
  if (!in.read(reinterpret_cast<char*>(&count), sizeof(count))) {
    std::cerr << "Snapshot: Failed to read entry count\n";
    return false;
  }

  // Read each entry
  for (uint32_t i = 0; i < count; i++) {
    // Key
    uint32_t key_len;
    if (!in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) {
      std::cerr << "Snapshot: Failed to read key length\n";
      return false;
    }
    std::string key(key_len, '\0');
    if (!in.read(key.data(), key_len)) {
      std::cerr << "Snapshot: Failed to read key\n";
      return false;
    }

    // Value
    uint32_t val_len;
    if (!in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len))) {
      std::cerr << "Snapshot: Failed to read value length\n";
      return false;
    }
    std::string value(val_len, '\0');
    if (!in.read(value.data(), val_len)) {
      std::cerr << "Snapshot: Failed to read value\n";
      return false;
    }

    // Version
    uint64_t version;
    if (!in.read(reinterpret_cast<char*>(&version), sizeof(version))) {
      std::cerr << "Snapshot: Failed to read version\n";
      return false;
    }

    data[key] = Entry{value, version};
  }

  return true;
}

bool Snapshot::Exists(const std::string& path) {
  return std::filesystem::exists(path);
}
