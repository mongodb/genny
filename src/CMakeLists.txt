set (src_files query.cpp parse_util.cpp insert.cpp workload.cpp node.cpp random_choice.cpp sleep.cpp forN.cpp parser)
add_library(big ${src_files})

add_executable(mwg parser.cpp)
target_link_libraries(mwg ${LIBMONGOCXX_LIBRARIES} ${YAMLCPP_LIBRARIES} big)


