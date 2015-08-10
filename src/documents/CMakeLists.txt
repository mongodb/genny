set(doc_files bsonDocument.cpp overrideDocument.cpp document.cpp) 
include_directories(..)
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
add_library(docs ${doc_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
