# 3.10.2 comes with Ubuntu 18.04.1 LTS
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
cmake_policy(VERSION 3.10)

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

find_package(Threads REQUIRED)

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
