cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(genny VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_EXTENSIONS                OFF)
set(CMAKE_SKIP_BUILD_RPATH              false)
set(CMAKE_BUILD_WITH_INSTALL_RPATH      false)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   true)
set(CMAKE_MACOSX_RPATH                  1) # Ensure that RPATH is used on OSX

# Help spot unknown compilers and versions.
# Other compilers are probably supported just fine but aren't officially checked
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        message(FATAL_ERROR "Insufficient Apple clang version - XCode 5.1+ required")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.0")
        message(FATAL_ERROR "Insufficient clang version - clang 4.0+ required")
    endif()
else()
    message(FATAL_ERROR "Unknown compiler... ${CMAKE_CXX_COMPILER_ID} version ${CMAKE_CXX_COMPILER_VERSION}")
endif()

enable_testing()

add_subdirectory(src)

