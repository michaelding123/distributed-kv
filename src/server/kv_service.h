#pragma once

#include <grpcpp/grpcpp.h>

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "kv.grpc.pb.h"

class KVServiceImpl final : public kv::KV::Service {
 public:
  grpc::Status Get(grpc::ServerContext*, const kv::GetRequest* req,
                   kv::GetResponse* resp) override {
    std::shared_lock lock(mu_);
    auto it = store_.find(req->key());
    if (it == store_.end()) {
      resp->set_found(false);
      return grpc::Status::OK;
    }
    resp->set_found(true);
    resp->set_value(it->second.value);
    resp->set_version(it->second.version);
    return grpc::Status::OK;
  }

  grpc::Status Put(grpc::ServerContext*, const kv::PutRequest* req,
                   kv::PutResponse* resp) override {
    std::unique_lock lock(mu_);
    auto& e = store_[req->key()];
    e.value = req->value();
    e.version++;
    resp->set_version(e.version);
    return grpc::Status::OK;
  }

  grpc::Status Delete(grpc::ServerContext*, const kv::DeleteRequest* req,
                      kv::DeleteResponse* resp) override {
    std::unique_lock lock(mu_);
    resp->set_ok(store_.erase(req->key()) > 0);
    return grpc::Status::OK;
  }

 private:
  struct Entry {
    std::string value;
    uint64_t version = 0;
  };
  std::unordered_map<std::string, Entry> store_;
  std::shared_mutex mu_;
};
