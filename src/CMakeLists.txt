set (src_files find.cpp parse_util.cpp insert.cpp workload.cpp node.cpp random_choice.cpp sleep.cpp forN.cpp)
add_library(big ${src_files})

set(CMAKE_CURRENT_BINARY_DIR  ${CMAKE_BINARY_DIR})

