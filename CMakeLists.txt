cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# https://cmake.org/cmake/help/v3.0/policy/CMP0025.html
if(POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif()

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

# If the user did not customize the install prefix,
# set it to <BUILD>/dist so we don't inadverently pollute /usr/local
if(${GENNY_DIST_DIR})
    set (CMAKE_INSTALL_PREFIX "${GENNY_DIST_DIR}" CACHE PATH "default install path" FORCE)
else()
    set (CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "default install path" FORCE)
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

include(GNUInstallDirs)

find_package(Threads REQUIRED)

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

# See ./cmake folder
include(GennyOptions)
include(GetBoost)

# Build the third party external pieces
include(GetLibMongoCxx)

enable_testing()
add_subdirectory(src)
