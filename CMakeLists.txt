# 3.10.2 comes with Ubuntu 18.04.1 LTS
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
cmake_policy(VERSION 3.10)

# Set the default output paths/prefixes
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib CACHE PATH "")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib CACHE PATH "")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin CACHE PATH "")
set(CMAKE_INSTALL_PREFIX            ${CMAKE_SOURCE_DIR}/dist CACHE PATH "")

# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(genny VERSION 0.0.1 LANGUAGES CXX)

# Include our local cmake folder in the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Use our modules
include(Options)
include(CheckCompiler)
include(ConfigureInstall)

find_package(Threads REQUIRED)

# Grab OpenSSL right away
include(GetSsl)

# <Boost>
if(GENNY_STATIC_BOOST_PATH)
    set(Boost_USE_STATIC_LIBS ON
        CACHE BOOL "Use static Boost"
    )
    set(Boost_NO_SYSTEM_PATHS ON
        CACHE BOOL "Use non-system Boost"
    )
    set(BOOST_ROOT "${GENNY_STATIC_BOOST_PATH}"
        CACHE PATH "Use a specific path for Boost"
    )
endif()

if(NOT Boost_USE_STATIC_LIBS)
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

# Required CMAKE options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS                OFF     CACHE BOOL "")
set(CMAKE_POSITION_INDEPENDENT_CODE     ON)

enable_testing()
add_subdirectory(src)
