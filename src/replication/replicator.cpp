#include "replicator.h"

#include <chrono>
#include <future>
#include <iostream>

Replicator::Replicator(const std::vector<std::string>& followers)
    : addresses_(followers) {
  for (const auto& addr : followers) {
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    stubs_.push_back(kv::KV::NewStub(channel));
    std::cout << "Replicator: Added follower " << addr << "\n";
  }
}

bool Replicator::ReplicatePut(const std::string& key, const std::string& value,
                              const std::string& request_id) {
  return Replicate(kv::OP_PUT, key, value, request_id);
}

bool Replicator::ReplicateDelete(const std::string& key,
                                 const std::string& request_id) {
  return Replicate(kv::OP_DELETE, key, "", request_id);
}

bool Replicator::Replicate(kv::OperationType op, const std::string& key,
                           const std::string& value,
                           const std::string& request_id) {
  if (stubs_.empty()) {
    return true;  // No followers, trivially successful
  }

  uint64_t seq = ++sequence_;

  // Build request
  kv::ReplicateRequest req;
  req.set_op(op);
  req.set_key(key);
  req.set_value(value);
  req.set_request_id(request_id);
  req.set_sequence(seq);

  // Send to all followers in parallel
  std::vector<std::future<bool>> futures;

  for (size_t i = 0; i < stubs_.size(); i++) {
    futures.push_back(std::async(std::launch::async, [this, i, &req]() {
      kv::ReplicateResponse resp;
      grpc::ClientContext ctx;

      // Set a timeout for replication
      ctx.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));

      auto status = stubs_[i]->Replicate(&ctx, req, &resp);

      if (!status.ok()) {
        std::cerr << "Replication to " << addresses_[i]
                  << " failed: " << status.error_message() << "\n";
        return false;
      }

      return resp.ok();
    }));
  }

  // Wait for responses and count successes
  size_t successes = 0;
  for (auto& f : futures) {
    if (f.get()) {
      successes++;
    }
  }

  // Require majority (more than half)
  size_t majority = (stubs_.size() / 2) + 1;
  bool ok = successes >= majority;

  if (!ok) {
    std::cerr << "Replication failed: only " << successes << "/" << stubs_.size()
              << " followers acked (need " << majority << ")\n";
  }

  return ok;
}
