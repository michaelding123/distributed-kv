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

add_executable(kv_server src/server/main.cpp)
target_include_directories(kv_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/server)
target_link_libraries(kv_server PRIVATE kv_proto)

add_executable(kv_client src/client/main.cpp)
target_link_libraries(kv_client PRIVATE kv_proto)

# Testing
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(storage_test tests/storage_test.cpp)
target_include_directories(storage_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/server)
target_link_libraries(storage_test PRIVATE kv_proto GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(storage_test)