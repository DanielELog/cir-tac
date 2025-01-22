set(LIBS
    MLIRCIR
    MLIRParser
    proto
)

add_library(protoSerializer
  EnumsSerializerGen.cpp
  Serializer.cpp
  SerializerGen.cpp
)

add_library(protoDeserializer
  Deserializer.cpp
  EnumsDeserializerGen.cpp
)

target_link_libraries(protoSerializer ${LIBS})
target_link_libraries(protoDeserializer ${LIBS})
