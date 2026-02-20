#pragma once

#include <grpcpp/grpcpp.h>

#include "kv.grpc.pb.h"
#include "storage/kv_store.h"

class Replicator;

class KVServiceImpl final : public kv::KV::Service {
 public:
  explicit KVServiceImpl(KVStore& store) : store_(store) {}

  // Set replicator for leader mode (optional)
  void SetReplicator(Replicator* replicator) { replicator_ = replicator; }

  // Returns true if this node is a leader (has a replicator)
  bool IsLeader() const { return replicator_ != nullptr; }

  grpc::Status Get(grpc::ServerContext*, const kv::GetRequest* req,
                   kv::GetResponse* resp) override {
    auto entry = store_.Get(req->key());
    if (!entry) {
      resp->set_found(false);
      return grpc::Status::OK;
    }
    resp->set_found(true);
    resp->set_value(entry->value);
    resp->set_version(entry->version);
    return grpc::Status::OK;
  }

  grpc::Status Put(grpc::ServerContext*, const kv::PutRequest* req,
                   kv::PutResponse* resp) override;

  grpc::Status Delete(grpc::ServerContext*, const kv::DeleteRequest* req,
                      kv::DeleteResponse* resp) override;

  grpc::Status Compact(grpc::ServerContext*, const kv::CompactRequest*,
                       kv::CompactResponse* resp) override {
    bool ok = store_.Compact();
    resp->set_ok(ok);
    return grpc::Status::OK;
  }

  // Replication RPC (called by leader on followers)
  grpc::Status Replicate(grpc::ServerContext*, const kv::ReplicateRequest* req,
                         kv::ReplicateResponse* resp) override;

 private:
  KVStore& store_;
  Replicator* replicator_ = nullptr;
};
