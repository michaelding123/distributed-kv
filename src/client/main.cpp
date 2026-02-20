#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

#include "kv.grpc.pb.h"

void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " [--server HOST:PORT] <command> [args...]\n"
            << "\n"
            << "Commands:\n"
            << "  get <key>                      Get value for key\n"
            << "  put <key> <value> [request_id] Store key-value pair\n"
            << "  delete <key> [request_id]      Delete key\n"
            << "  compact                        Compact WAL (snapshot + truncate)\n"
            << "\n"
            << "Options:\n"
            << "  --server, -s HOST:PORT  Server address (default: localhost:50051)\n"
            << "\n"
            << "Examples:\n"
            << "  " << prog << " put mykey myvalue\n"
            << "  " << prog << " get mykey\n"
            << "  " << prog << " delete mykey\n"
            << "  " << prog << " -s localhost:9090 get mykey\n";
}

int main(int argc, char** argv) {
  std::string server = "localhost:50051";
  int arg_idx = 1;

  // Parse optional --server flag
  while (arg_idx < argc) {
    std::string arg = argv[arg_idx];
    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg == "--server" || arg == "-s") {
      if (arg_idx + 1 >= argc) {
        std::cerr << "Error: --server requires an argument\n";
        return 1;
      }
      server = argv[arg_idx + 1];
      arg_idx += 2;
    } else if (arg[0] == '-') {
      std::cerr << "Error: Unknown option: " << arg << "\n";
      PrintUsage(argv[0]);
      return 1;
    } else {
      break;
    }
  }

  // Need at least a command
  if (arg_idx >= argc) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string command = argv[arg_idx++];

  // Connect to server
  auto channel = grpc::CreateChannel(server, grpc::InsecureChannelCredentials());
  auto stub = kv::KV::NewStub(channel);

  if (command == "get") {
    if (arg_idx >= argc) {
      std::cerr << "Error: get requires a key\n";
      return 1;
    }
    std::string key = argv[arg_idx];

    kv::GetRequest req;
    req.set_key(key);
    kv::GetResponse resp;
    grpc::ClientContext ctx;

    auto status = stub->Get(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "Error: " << status.error_message() << "\n";
      return 1;
    }

    if (resp.found()) {
      std::cout << resp.value() << "\n";
    } else {
      std::cerr << "Key not found: " << key << "\n";
      return 1;
    }

  } else if (command == "put") {
    if (arg_idx + 1 >= argc) {
      std::cerr << "Error: put requires key and value\n";
      return 1;
    }
    std::string key = argv[arg_idx++];
    std::string value = argv[arg_idx++];
    std::string request_id;
    if (arg_idx < argc) {
      request_id = argv[arg_idx];
    }

    kv::PutRequest req;
    req.set_key(key);
    req.set_value(value);
    req.set_request_id(request_id);
    kv::PutResponse resp;
    grpc::ClientContext ctx;

    auto status = stub->Put(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "Error: " << status.error_message() << "\n";
      return 1;
    }

    std::cout << "OK (version " << resp.version() << ")\n";

  } else if (command == "delete") {
    if (arg_idx >= argc) {
      std::cerr << "Error: delete requires a key\n";
      return 1;
    }
    std::string key = argv[arg_idx++];
    std::string request_id;
    if (arg_idx < argc) {
      request_id = argv[arg_idx];
    }

    kv::DeleteRequest req;
    req.set_key(key);
    req.set_request_id(request_id);
    kv::DeleteResponse resp;
    grpc::ClientContext ctx;

    auto status = stub->Delete(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "Error: " << status.error_message() << "\n";
      return 1;
    }

    if (resp.ok()) {
      std::cout << "Deleted\n";
    } else {
      std::cerr << "Key not found: " << key << "\n";
      return 1;
    }

  } else if (command == "compact") {
    kv::CompactRequest req;
    kv::CompactResponse resp;
    grpc::ClientContext ctx;

    auto status = stub->Compact(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "Error: " << status.error_message() << "\n";
      return 1;
    }

    if (resp.ok()) {
      std::cout << "Compacted\n";
    } else {
      std::cerr << "Compaction failed\n";
      return 1;
    }

  } else {
    std::cerr << "Error: Unknown command: " << command << "\n";
    PrintUsage(argv[0]);
    return 1;
  }

  return 0;
}
