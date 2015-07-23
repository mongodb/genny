set (src_files query.cpp parse_util.cpp insert.cpp workload.cpp node.cpp random_choice.cpp sleep.cpp forN.cpp parser)
add_library(big ${src_files})

set(CMAKE_CURRENT_BINARY_DIR  ${CMAKE_BINARY_DIR})

