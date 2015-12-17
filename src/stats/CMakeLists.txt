set(stats_files stats.cpp)
include_directories(..)
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
add_library(stats ${stats_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
