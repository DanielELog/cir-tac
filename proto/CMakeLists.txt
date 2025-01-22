include(FindProtobuf)
find_package(Protobuf REQUIRED CONFIG)
include_directories(${PROTOBUF_INCLUDE_DIR})

set(PROTOS
    enumgen.proto
    model.proto
    opgen.proto
    setup.proto
    type.proto
)

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ${PROTOS})
add_library(proto ${PROTO_HEADER} ${PROTO_SRC})
target_link_libraries(
    proto ${Protobuf_LIBRARIES}
    protobuf::libprotobuf
    absl::log_internal_message
    absl::log_internal_check_op
    absl::raw_logging_internal
    absl::spinlock_wait
)
