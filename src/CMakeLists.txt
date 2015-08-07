set(src_files find.cpp parse_util.cpp insert_one.cpp insert_many.cpp
        workload.cpp node.cpp random_choice.cpp sleep.cpp forN.cpp
        opNode.cpp) 
add_library(big ${src_files})

include_directories(documents)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
add_subdirectory(documents)
