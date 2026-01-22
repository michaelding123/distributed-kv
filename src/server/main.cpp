#include "kv_service.h"

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::string addr = "0.0.0.0:50051";
  if (argc > 1) addr = argv[1];

  KVStore store;
  KVServiceImpl service(store);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  auto server = builder.BuildAndStart();
  std::cout << "KV server listening on " << addr << "\n";
  server->Wait();
  return 0;
}
