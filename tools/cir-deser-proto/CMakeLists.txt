set(LIBS
    protoSerializer
)

add_executable(cir-deser-proto cir-deser-proto.cpp)

target_link_libraries(cir-deser-proto ${LIBS})
