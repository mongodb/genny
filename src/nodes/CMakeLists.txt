set(src_files finish_node.cpp node.cpp forN.cpp opNode.cpp
random_choice.cpp sleep.cpp doAll.cpp join.cpp workloadNode.cpp
ifNode.cpp spawn.cpp) 
include_directories(..)
add_library(nodes ${src_files})
include_directories(documents nodes operations)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
