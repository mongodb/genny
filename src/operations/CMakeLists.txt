set(src_files find.cpp insert_many.cpp insert_one.cpp count.cpp
delete_one.cpp delete_many.cpp create_index.cpp) 
add_library(operations ${src_files})
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
include_directories(documents nodes operations)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
