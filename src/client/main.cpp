#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

#include "kv.grpc.pb.h"

int main(int argc, char** argv) {
  std::string target = "localhost:50051";
  if (argc > 1) target = argv[1];

  auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  std::unique_ptr<kv::KV::Stub> stub = kv::KV::NewStub(channel);

  // Put
  kv::PutRequest put;
  put.set_key("hello");
  put.set_value("world");
  put.set_request_id("req-1");
  kv::PutResponse putResp;
  grpc::ClientContext putCtx;
  auto s1 = stub->Put(&putCtx, put, &putResp);
  std::cout << "Put status=" << s1.ok() << " version=" << putResp.version() << "\n";

  // Get
  kv::GetRequest get;
  get.set_key("hello");
  kv::GetResponse getResp;
  grpc::ClientContext getCtx;
  auto s2 = stub->Get(&getCtx, get, &getResp);
  std::cout << "Get status=" << s2.ok() << " found=" << getResp.found()
            << " value=" << getResp.value() << " version=" << getResp.version() << "\n";

  return 0;
}