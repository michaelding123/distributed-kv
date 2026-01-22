#pragma once

#include <grpcpp/grpcpp.h>

#include "kv.grpc.pb.h"
#include "storage/kv_store.h"

class KVServiceImpl final : public kv::KV::Service {
 public:
  explicit KVServiceImpl(KVStore& store) : store_(store) {}

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
                   kv::PutResponse* resp) override {
    uint64_t version = store_.Put(req->key(), req->value());
    resp->set_version(version);
    return grpc::Status::OK;
  }

  grpc::Status Delete(grpc::ServerContext*, const kv::DeleteRequest* req,
                      kv::DeleteResponse* resp) override {
    resp->set_ok(store_.Delete(req->key()));
    return grpc::Status::OK;
  }

 private:
  KVStore& store_;
};
