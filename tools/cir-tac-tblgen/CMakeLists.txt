set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

set(LIBS
  MLIRTblgenLib
)

add_executable(cir-tac-tblgen
  cir-tac-tblgen.cpp
  VespaCommon.cpp
  VespaAttrGen.cpp
  VespaEnumGen.cpp
  VespaGen.cpp
  VespaOpGen.cpp
  VespaTypeGen.cpp
)

target_link_libraries(cir-tac-tblgen ${LIBS})
