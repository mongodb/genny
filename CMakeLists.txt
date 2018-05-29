cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(genny VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_EXTENSIONS                OFF)
set(CMAKE_SKIP_BUILD_RPATH              false)
set(CMAKE_BUILD_WITH_INSTALL_RPATH      false)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   true)
set(CMAKE_MACOSX_RPATH                  1) # Ensure that RPATH is used on OSX

set(THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)

enable_testing()

add_subdirectory(src)

