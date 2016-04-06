cmake_minimum_required (VERSION 2.6)
project (WorkloadGeneration)

# The version number.
set (WorkloadGeneration_VERSION_MAJOR 0)
set (WorkloadGeneration_VERSION_MINOR 4)
set (WorkloadGeneration_VERSION_PATCH 0)

# Additional cmake modules: git stuff and findyaml
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/rpavlik/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/third_party/findyaml")

# Use git cmake module from rpavlik to put git hash into variable for
# use by version option. Forces an update and a link on commit
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1) 
#Shift to use following when we have tags 
#git_describe(GIT_SHA1)


# CPack stuff. CPack used to make packages. (make package)
SET(CPACK_PACKAGE_VERSION_MAJOR ${WorkloadGeneration_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${WorkloadGeneration_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${WorkloadGeneration_VERSION_PATCH})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Workload Generator for MongoDB")
SET(CPACK_PACKAGE_DESCRIPTION_FILE
"${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

# Strict error checking + c++11
add_definitions(-Wall)
add_definitions(-Werror)
add_definitions(-std=c++11)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# We have some generated header/cpp files. 
include_directories("${PROJECT_BINARY_DIR}")

find_package(PkgConfig REQUIRED)

# yaml-cpp support
set(YAMLCPP_STATIC_LIBRARY TRUE)
find_package(YamlCpp REQUIRED)
if (!${YAMLCPP_FOUND})
   message(FATAL_ERROR "yaml-cpp not found")
endif()

# Mongo c++11 driver support
pkg_check_modules(LIBMONGOCXX  libmongocxx)
if (!${LIBMONGOCXX_FOUND})
   message(FATAL_ERROR "libmongocxx not found")
endif()

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${YAMLCPP_INCLUDE_DIR})

# Try to find static libraries if they exist. Fall back to dynamic if
# no static libraries.
foreach(lib ${LIBMONGOCXX_STATIC_LIBRARIES})
            find_library(${lib}-LIBRARY NAMES lib${lib}.a ${lib}  HINT ${LIBMONGOCXX_LIBRARY_DIRS})
            set(LIBRARIES ${LIBRARIES} ${${lib}-LIBRARY})
endforeach(lib)

link_directories(${LIBMONGOCXX_LIBRARY_DIRS} ${YAMLCPP_LIBRARY_DIR})

find_package(Threads REQUIRED)

# Allow someone to specify non static libs at the command line.
option(Boost_NON_STATIC "Disable use of static libraries for BOOST" OFF)
if (NOT Boost_NON_STATIC)
   MESSAGE("Using Boost Static Libs")
   set(Boost_USE_STATIC_LIBS   ON)
else()
   MESSAGE("Boost_NON_STATIC specified on command line -- using dynamic")
   ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)
endif()

find_package(Boost 1.54 REQUIRED COMPONENTS log thread filesystem
regex system)
include_directories(${Boost_INCLUDE_DIRS})

# Generate main.h with project version information
configure_file (
 "${PROJECT_SOURCE_DIR}/src/main.h.in"
  "${PROJECT_BINARY_DIR}/main.h"
)

# Generate file with current git version information
configure_file (
 "${PROJECT_SOURCE_DIR}/src/githash.cpp.in"
  "${PROJECT_BINARY_DIR}/githash.cpp"
)

# Support ctest and make test target 
enable_testing()

add_subdirectory(src)
include_directories(src/stats)
add_executable(mwg src/main.cpp githash.cpp)
target_link_libraries(mwg workload nodes operations
docs values parse stats ${Boost_LIBRARIES} ${LIBRARIES} ${YAMLCPP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(examples)

install(TARGETS mwg DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION
"examples" FILES_MATCHING PATTERN "*.yml")
