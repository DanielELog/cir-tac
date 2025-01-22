set(LIBS
    proto
    protoDeserializer
    clangTooling
    MLIRCIR
)

add_definitions("-g")

add_executable(cir-deser-proto cir-deser-proto.cpp)

target_link_libraries(cir-deser-proto ${LIBS})
