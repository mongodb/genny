cmake_minimum_required (VERSION 2.6)
project (WorkloadGeneration)

# The version number.
set (WorkloadGeneration_VERSION_MAJOR -1)
set (WorkloadGeneration_VERSION_MINOR 0)

add_definitions(-MMD)
add_definitions(-Wall)
add_definitions(-Werror)
add_definitions(-std=c++11)
add_definitions(-g)
add_definitions(-O3)

set (src_files query.cpp parse_util.cpp insert.cpp workload.cpp node.cpp random_choice.cpp sleep.cpp forN.cpp parser)

find_package(PkgConfig)
pkg_check_modules(LIBMONGOCXX  libmongocxx)
pkg_check_modules(YAMLCPP  yaml-cpp)

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${YAMLCPP_INCLUDE_DIRS})

add_library(big ${src_files})

add_executable(mwg parser.cpp)
target_link_libraries(mwg ${LIBMONGOCXX_LIBRARIES} ${YAMLCPP_LIBRARIES} big)
