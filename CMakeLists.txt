cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
cmake_policy(VERSION 3.11)

# https://cmake.org/cmake/help/v3.0/ipolicy/CMP0042.html
if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

# https://cmake.org/cmake/help/v3.13/policy/CMP0077.html
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(genny VERSION 0.0.1 LANGUAGES CXX)

# Include our local cmake folder in the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Use our modules
include(Options)
include(CheckCompiler)
include(ConfigureInstall)

if(ENABLE_SSL)
    include(GetSsl)
endif()

# Use other external modules
include(GNUInstallDirs)

find_package(Threads REQUIRED)

# <Boost>
set(Boost_USE_STATIC_LIBS ${USE_STATIC_BOOST})
if(NOT ${USE_STATIC_BOOST})
  add_definitions(-DBOOST_ALL_DYN_LINK)
endif()

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(
        Boost
        1.58 # Minimum version, vetted via Ubuntu 16.04
        REQUIRED
        COMPONENTS
        log_setup
        log
        program_options
)
# </Boost>

# <Mongo Driver>
find_package(libmongocxx REQUIRED)
# </Mongo Driver>

# Required CMAKE options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS                OFF     CACHE BOOL "")
set(CMAKE_POSITION_INDEPENDENT_CODE     ON)

# These libraries are almost ubiquitously required in genny
# The AFTER tag means that our headers are considered before headers in the same folder as these
# dependencies
include_directories(AFTER
    ${LIBMONGOCXX_INCLUDE_DIRS}
)
link_libraries(
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBMONGOCXX_LIBRARIES}
)

enable_testing()
add_subdirectory(src)
