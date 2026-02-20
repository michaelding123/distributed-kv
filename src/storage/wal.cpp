#include "wal.h"

#include <stdexcept>

// WAL entry format (binary):
//   [1 byte]  type (PUT=1, DELETE=2)
//   [4 bytes] key length
//   [N bytes] key
//   [4 bytes] value length (0 for DELETE)
//   [N bytes] value
//   [4 bytes] request_id length
//   [N bytes] request_id

WAL::WAL(const std::string& path) : path_(path) {
  out_.open(path_, std::ios::binary | std::ios::app);
  if (!out_) {
    throw std::runtime_error("Failed to open WAL file: " + path_);
  }
}

WAL::~WAL() {
  if (out_.is_open()) {
    out_.close();
  }
}

void WAL::LogPut(const std::string& key, const std::string& value,
                 const std::string& request_id) {
  WALEntry entry{WALEntryType::PUT, key, value, request_id};
  WriteEntry(entry);
}

void WAL::LogDelete(const std::string& key, const std::string& request_id) {
  WALEntry entry{WALEntryType::DELETE, key, "", request_id};
  WriteEntry(entry);
}

void WAL::WriteEntry(const WALEntry& entry) {
  std::lock_guard<std::mutex> lock(mu_);

  // Type
  uint8_t type = static_cast<uint8_t>(entry.type);
  out_.write(reinterpret_cast<const char*>(&type), sizeof(type));

  // Key
  uint32_t key_len = static_cast<uint32_t>(entry.key.size());
  out_.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
  out_.write(entry.key.data(), entry.key.size());

  // Value
  uint32_t val_len = static_cast<uint32_t>(entry.value.size());
  out_.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
  out_.write(entry.value.data(), entry.value.size());

  // Request ID
  uint32_t req_len = static_cast<uint32_t>(entry.request_id.size());
  out_.write(reinterpret_cast<const char*>(&req_len), sizeof(req_len));
  out_.write(entry.request_id.data(), entry.request_id.size());

  out_.flush();
}

void WAL::Sync() {
  std::lock_guard<std::mutex> lock(mu_);
  out_.flush();
}

void WAL::Reset() {
  std::lock_guard<std::mutex> lock(mu_);
  out_.close();
  // Reopen in truncate mode to clear the file
  out_.open(path_, std::ios::binary | std::ios::trunc);
  if (!out_) {
    throw std::runtime_error("Failed to reset WAL file: " + path_);
  }
}

std::vector<WALEntry> WAL::ReadAll() {
  std::vector<WALEntry> entries;

  std::ifstream in(path_, std::ios::binary);
  if (!in) {
    // No existing WAL file, return empty
    return entries;
  }

  while (in.peek() != EOF) {
    try {
      entries.push_back(ReadEntry(in));
    } catch (const std::exception&) {
      // Truncated entry at end of file, stop reading
      break;
    }
  }

  return entries;
}

WALEntry WAL::ReadEntry(std::ifstream& in) {
  WALEntry entry;

  // Type
  uint8_t type;
  if (!in.read(reinterpret_cast<char*>(&type), sizeof(type))) {
    throw std::runtime_error("Failed to read entry type");
  }
  entry.type = static_cast<WALEntryType>(type);

  // Key
  uint32_t key_len;
  if (!in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) {
    throw std::runtime_error("Failed to read key length");
  }
  entry.key.resize(key_len);
  if (!in.read(entry.key.data(), key_len)) {
    throw std::runtime_error("Failed to read key");
  }

  // Value
  uint32_t val_len;
  if (!in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len))) {
    throw std::runtime_error("Failed to read value length");
  }
  entry.value.resize(val_len);
  if (!in.read(entry.value.data(), val_len)) {
    throw std::runtime_error("Failed to read value");
  }

  // Request ID
  uint32_t req_len;
  if (!in.read(reinterpret_cast<char*>(&req_len), sizeof(req_len))) {
    throw std::runtime_error("Failed to read request_id length");
  }
  entry.request_id.resize(req_len);
  if (!in.read(entry.request_id.data(), req_len)) {
    throw std::runtime_error("Failed to read request_id");
  }

  return entry;
}
