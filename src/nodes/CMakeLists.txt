set(src_files finish_node.cpp node.cpp forN.cpp opNode.cpp
random_choice.cpp sleep.cpp doAll.cpp join.cpp workloadNode.cpp ifNode.cpp) 
include_directories(..)
add_library(nodes ${src_files})
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
include_directories(documents nodes operations)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
