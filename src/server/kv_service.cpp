#include "kv_service.h"

#include <iostream>

#include "replication/replicator.h"

grpc::Status KVServiceImpl::Put(grpc::ServerContext*,
                                const kv::PutRequest* req,
                                kv::PutResponse* resp) {
  std::cout << "Put(" << req->key() << ") - replicator=" << (replicator_ ? "yes" : "no") << "\n";

  // Apply locally first
  auto result = store_.Put(req->key(), req->value(), req->request_id());
  resp->set_version(result.version);

  std::cout << "  Applied locally, version=" << result.version
            << ", was_duplicate=" << result.was_duplicate << "\n";

  // If we're the leader and this wasn't a duplicate, replicate to followers
  if (replicator_ && !result.was_duplicate) {
    std::cout << "  Replicating to followers...\n";
    bool ok = replicator_->ReplicatePut(req->key(), req->value(), req->request_id());
    if (!ok) {
      std::cerr << "Warning: Replication failed for Put(" << req->key() << ")\n";
    } else {
      std::cout << "  Replication succeeded\n";
    }
  }

  return grpc::Status::OK;
}

grpc::Status KVServiceImpl::Delete(grpc::ServerContext*,
                                   const kv::DeleteRequest* req,
                                   kv::DeleteResponse* resp) {
  // Apply locally first
  auto result = store_.Delete(req->key(), req->request_id());
  resp->set_ok(result.ok);

  // If we're the leader and this wasn't a duplicate, replicate to followers
  if (replicator_ && !result.was_duplicate) {
    bool ok = replicator_->ReplicateDelete(req->key(), req->request_id());
    if (!ok) {
      std::cerr << "Warning: Replication failed for Delete(" << req->key() << ")\n";
    }
  }

  return grpc::Status::OK;
}

grpc::Status KVServiceImpl::Replicate(grpc::ServerContext*,
                                      const kv::ReplicateRequest* req,
                                      kv::ReplicateResponse* resp) {
  // This is called on followers by the leader
  // Apply the operation to local store

  switch (req->op()) {
    case kv::OP_PUT: {
      auto result = store_.Put(req->key(), req->value(), req->request_id());
      resp->set_ok(true);
      resp->set_sequence(req->sequence());
      break;
    }
    case kv::OP_DELETE: {
      auto result = store_.Delete(req->key(), req->request_id());
      resp->set_ok(true);
      resp->set_sequence(req->sequence());
      break;
    }
    default:
      resp->set_ok(false);
      return grpc::Status(grpc::INVALID_ARGUMENT, "Unknown operation type");
  }

  return grpc::Status::OK;
}
