set(stats_files stats.cpp)
include_directories(..)
add_library(stats ${stats_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
