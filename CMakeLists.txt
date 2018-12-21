cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# https://cmake.org/cmake/help/v3.0/policy/CMP0025.html
if(POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif()

# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(genny VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Threads REQUIRED)

# <Boost>
option(USE_STATIC_BOOST
       "Use boost static libaries when compiling genny. \
        This also requires boost static libraries to have position-independent code."
       OFF)

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
add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
# </Boost>

# <Mongo Driver>
find_package(libmongocxx REQUIRED)
# </Mongo Driver>

# <gRPC>
# Add our third_party path to the potential places to find gRPCConfig.cmake
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/grpc")
find_package(gRPC REQUIRED)
# </gRPC>

# <Misc 3rd Party Integrations>
set(THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)
# </Misc 3rd Party Integrations>

# Required CMAKE options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE     ON)

# Nice-to-have CMAKE options
set(CMAKE_CXX_EXTENSIONS                OFF     CACHE BOOL "")
set(CMAKE_SKIP_BUILD_RPATH              false   CACHE BOOL "")
set(CMAKE_BUILD_WITH_INSTALL_RPATH      false   CACHE BOOL "")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   true    CACHE BOOL "")
set(CMAKE_MACOSX_RPATH                  ON      CACHE BOOL "Ensure that RPATH is used on OSX")

# This line allows us to run genny executables from a distribution folder where libraries live in
# bin/../lib64. This is a nice thing for driver-like applications, because you can pass around
# archives trivially.
set(CMAKE_INSTALL_RPATH                 "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"   CACHE PATH "")

# Help spot unknown compilers and versions.
# Other compilers are probably supported just fine but aren't officially checked
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "Insufficient Apple clang version - XCode 10+ required")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6.0")
        message(FATAL_ERROR "Insufficient clang version - clang 6.0+ required")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.3.0")
        message(FATAL_ERROR "Insufficient GCC/GNU version - gnu/gcc 7.3.0+ required")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.14.26430")
        message(FATAL_ERROR "Insufficient Microsoft Visual C++ version - VS 2017 15.7+ (compiler version 19.14.26430). Found compiler version ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
else()
    message(FATAL_ERROR "Unknown compiler... ${CMAKE_CXX_COMPILER_ID} version ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# If the user did not customize the install prefix to ${genny_root}/install
# set it to live under build so we don't inadverently pollute /usr/local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
endif()

# These libraries are almost ubiquitously required in genny
# The AFTER tag means that our headers are considered before headers in the same folder as these
# dependencies
include_directories(AFTER
    ${Boost_INCLUDE_DIR}
    ${LIBMONGOCXX_INCLUDE_DIRS}
)
link_libraries(
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    ${LIBMONGOCXX_LIBRARIES}
)

enable_testing()
add_subdirectory(src)
