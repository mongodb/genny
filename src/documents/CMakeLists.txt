set(doc_files bsonDocument.cpp overrideDocument.cpp document.cpp
template_document.cpp)

include_directories(values)
add_library(docs ${doc_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
