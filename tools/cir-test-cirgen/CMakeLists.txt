add_definitions("-g")

add_executable(cir-test-cirgen cir-test-cirgen.cpp)

target_link_libraries(cir-test-cirgen ${LIBS} clangTooling MLIRCIR MLIRMemRefDialect)
