#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"

// Replicator sends write operations from leader to followers
class Replicator {
 public:
  // Create a replicator with a list of follower addresses
  // e.g., {"localhost:50052", "localhost:50053"}
  explicit Replicator(const std::vector<std::string>& followers);

  // Replicate a PUT operation to all followers
  // Returns true if a majority of followers acknowledged
  bool ReplicatePut(const std::string& key, const std::string& value,
                    const std::string& request_id);

  // Replicate a DELETE operation to all followers
  // Returns true if a majority of followers acknowledged
  bool ReplicateDelete(const std::string& key, const std::string& request_id);

  // Get the number of connected followers
  size_t FollowerCount() const { return stubs_.size(); }

 private:
  bool Replicate(kv::OperationType op, const std::string& key,
                 const std::string& value, const std::string& request_id);

  std::vector<std::unique_ptr<kv::KV::Stub>> stubs_;
  std::vector<std::string> addresses_;
  std::atomic<uint64_t> sequence_{0};
  mutable std::mutex mu_;
};
