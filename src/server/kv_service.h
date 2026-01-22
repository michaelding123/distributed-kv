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
    auto result = store_.Put(req->key(), req->value(), req->request_id());
    resp->set_version(result.version);
    return grpc::Status::OK;
  }

  grpc::Status Delete(grpc::ServerContext*, const kv::DeleteRequest* req,
                      kv::DeleteResponse* resp) override {
    auto result = store_.Delete(req->key(), req->request_id());
    resp->set_ok(result.ok);
    return grpc::Status::OK;
  }

 private:
  KVStore& store_;
};
