cmake_minimum_required(VERSION 3.10)

project(squeeze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Only works on gcc
set(CMAKE_C_COMPILER    gcc-8)
set(CMAKE_CXX_COMPILER  g++-8)

set (CMAKE_SKIP_BUILD_RPATH false)
set (CMAKE_BUILD_WITH_INSTALL_RPATH false)
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH true)
set(CMAKE_MACOSX_RPATH 1) # Ensure that RPATH is used on OSX

set(CMAKE_CXX_EXTENSIONS OFF)


add_subdirectory(src)
