cmake_minimum_required (VERSION 2.6)
project (WorkloadGeneration)

# Use git cmake module from rpavlik
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/rpavlik/cmake")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

# The version number.
set (WorkloadGeneration_VERSION_MAJOR 0)
set (WorkloadGeneration_VERSION_MINOR 3)
set (WorkloadGeneration_VERSION_PATCH 0)


# CPack stuff
SET(CPACK_PACKAGE_VERSION_MAJOR ${WorkloadGeneration_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${WorkloadGeneration_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${WorkloadGeneration_VERSION_PATCH})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Workload Generator for MongoDB")
SET(CPACK_PACKAGE_DESCRIPTION_FILE
"${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

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

configure_file (
 "${PROJECT_SOURCE_DIR}/src/githash.cpp.in"
  "${PROJECT_BINARY_DIR}/githash.cpp"
)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
enable_testing()
add_subdirectory(src)
include_directories(src/stats)
add_executable(mwg src/main.cpp githash.cpp)
target_link_libraries(mwg workload nodes operations
docs parse stats ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES}
${YAMLCPP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(examples)


install(TARGETS mwg DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "example")

