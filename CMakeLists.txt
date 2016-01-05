cmake_minimum_required (VERSION 2.6)
project (WorkloadGeneration)

# The version number.
set (WorkloadGeneration_VERSION_MAJOR 0)
set (WorkloadGeneration_VERSION_MINOR 2)

add_definitions(-Wall)
add_definitions(-Werror)
add_definitions(-std=c++11)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

include_directories("${PROJECT_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMONGOCXX  libmongocxx)
if (!${LIBMONGOCXX_FOUND}) 
   message(FARAL_ERROR "libmongocxx not found")
endif()
pkg_check_modules(YAMLCPP  yaml-cpp)
if (!${YAMLCPP_FOUND}) 
   message(FARAL_ERROR "yaml-cpp not found")
endif()

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${YAMLCPP_INCLUDE_DIRS})
link_directories(${LIBMONGOCXX_LIBRARY_DIRS} ${YAMLCPP_LIBRARY_DIRS})

find_package(Threads REQUIRED)

find_package(Git)
if(GIT_FOUND)
        
  # Get the current working branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Get the latest abbreviated commit hash of the working branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

find_package(Boost 1.59 REQUIRED COMPONENTS log thread filesystem regex)
include_directories(${Boost_INCLUDE_DIRS})

configure_file (
 "${PROJECT_SOURCE_DIR}/src/main.h.in"
  "${PROJECT_BINARY_DIR}/main.h"
)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
enable_testing()
add_subdirectory(src)
include_directories(src/stats)
add_executable(mwg src/main.cpp)
target_link_libraries(mwg workload nodes operations
docs parse stats ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES}
${YAMLCPP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


install(TARGETS mwg DESTINATION bin)
add_subdirectory(examples)

