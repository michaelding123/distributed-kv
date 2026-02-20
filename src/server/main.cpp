#include "kv_service.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "replication/replicator.h"

void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " [options]\n"
            << "\n"
            << "Options:\n"
            << "  --addr ADDR       Listen address (default: 0.0.0.0:50051)\n"
            << "  --wal PATH        WAL file path (default: kv.wal)\n"
            << "  --role ROLE       Role: leader or follower (default: leader)\n"
            << "  --peers ADDRS     Comma-separated follower addresses (leader only)\n"
            << "  --help            Show this help\n"
            << "\n"
            << "Examples:\n"
            << "  # Start a standalone server (single node)\n"
            << "  " << prog << "\n"
            << "\n"
            << "  # Start a leader with two followers\n"
            << "  " << prog << " --addr 0.0.0.0:50051 --role leader --peers localhost:50052,localhost:50053\n"
            << "\n"
            << "  # Start a follower\n"
            << "  " << prog << " --addr 0.0.0.0:50052 --wal kv2.wal --role follower\n";
}

std::vector<std::string> SplitString(const std::string& s, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    if (!item.empty()) {
      result.push_back(item);
    }
  }
  return result;
}

int main(int argc, char** argv) {
  std::string addr = "0.0.0.0:50051";
  std::string wal_path = "kv.wal";
  std::string role = "leader";
  std::vector<std::string> peers;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg == "--addr" && i + 1 < argc) {
      addr = argv[++i];
    } else if (arg == "--wal" && i + 1 < argc) {
      wal_path = argv[++i];
    } else if (arg == "--role" && i + 1 < argc) {
      role = argv[++i];
      if (role != "leader" && role != "follower") {
        std::cerr << "Error: --role must be 'leader' or 'follower'\n";
        return 1;
      }
    } else if (arg == "--peers" && i + 1 < argc) {
      peers = SplitString(argv[++i], ',');
    } else {
      std::cerr << "Error: Unknown option: " << arg << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  std::cout << "Starting KV server...\n";
  std::cout << "  Address: " << addr << "\n";
  std::cout << "  WAL: " << wal_path << "\n";
  std::cout << "  Role: " << role << "\n";

  if (role == "leader" && !peers.empty()) {
    std::cout << "  Peers: ";
    for (size_t i = 0; i < peers.size(); i++) {
      if (i > 0) std::cout << ", ";
      std::cout << peers[i];
    }
    std::cout << "\n";
  }

  // Enable gRPC reflection
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  // Create store and service
  KVStore store(wal_path);
  KVServiceImpl service(store);

  // If leader with peers, create replicator
  std::unique_ptr<Replicator> replicator;
  if (role == "leader" && !peers.empty()) {
    replicator = std::make_unique<Replicator>(peers);
    service.SetReplicator(replicator.get());
    std::cout << "Leader mode: replicating to " << peers.size() << " follower(s)\n";
  } else if (role == "follower") {
    std::cout << "Follower mode: accepting replication from leader\n";
  } else {
    std::cout << "Standalone mode: no replication\n";
  }

  // Build and start server
  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  auto server = builder.BuildAndStart();
  std::cout << "KV server listening on " << addr << "\n";
  server->Wait();

  return 0;
}
