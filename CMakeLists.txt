cmake_minimum_required(VERSION 3.20)
project(distributed_kv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto files
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/kv.proto)

# Where generated .pb/.grpc.pb files will go
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generated sources
set(PROTO_SRCS ${GENERATED_DIR}/kv.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/kv.pb.h)
set(GRPC_SRCS  ${GENERATED_DIR}/kv.grpc.pb.cc)
set(GRPC_HDRS  ${GENERATED_DIR}/kv.grpc.pb.h)

# Generate protobuf + gRPC code using protoc + grpc_cpp_plugin
add_custom_command(
  OUTPUT
    ${PROTO_SRCS} ${PROTO_HDRS}
    ${GRPC_SRCS}  ${GRPC_HDRS}
  COMMAND protobuf::protoc
  ARGS
    --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
    --cpp_out=${GENERATED_DIR}
    --grpc_out=${GENERATED_DIR}
    --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    kv.proto
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating protobuf and gRPC sources from kv.proto"
  VERBATIM
)

add_library(kv_proto
  ${PROTO_SRCS}
  ${PROTO_HDRS}
  ${GRPC_SRCS}
  ${GRPC_HDRS}
)

target_include_directories(kv_proto
  PUBLIC
    ${GENERATED_DIR}
)

target_link_libraries(kv_proto
  PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

# Storage library
add_library(kv_storage
  src/storage/kv_store.cpp
  src/storage/wal.cpp
  src/storage/snapshot.cpp
)
target_include_directories(kv_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/storage ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Replication library
add_library(kv_replication
  src/replication/replicator.cpp
)
target_include_directories(kv_replication PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kv_replication PUBLIC kv_proto)

# Service library
add_library(kv_service
  src/server/kv_service.cpp
)
target_include_directories(kv_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kv_service PUBLIC kv_proto kv_storage kv_replication)

add_executable(kv_server src/server/main.cpp)
target_include_directories(kv_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kv_server PRIVATE kv_service gRPC::grpc++_reflection)

add_executable(kv_client src/client/main.cpp)
target_link_libraries(kv_client PRIVATE kv_proto)

# Testing
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(storage_test tests/storage_test.cpp)
target_include_directories(storage_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(storage_test PRIVATE kv_service GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(storage_test)