# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(gennylib VERSION 0.0.1 LANGUAGES CXX)

#Pacify the CLion
file(GLOB_RECURSE HEADERS_FOR_CLION
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     CONFIGURE_DEPENDS
     include/*.hpp include/*.hh src/*.hh)
add_library(gennylib_headers SHARED
    EXCLUDE_FROM_ALL
    src/version.cpp
    ${HEADERS_FOR_CLION}
)

add_library(gennylib_core STATIC
    src/version.cpp
)
target_include_directories(gennylib_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(gennylib_core
    yaml-cpp
    loki
    metrics
    value_generators
    Boost::boost Boost::log
    MongoCxx::mongocxx
)

# Concepts to do with Actors
add_library(gennylib_cast STATIC
    src/Actor.cpp
    src/ActorProducer.cpp
    src/Cast.cpp
    src/ExecutionStrategy.cpp
    src/RateLimiter.cpp
)
target_link_libraries(gennylib_cast gennylib_core)

# Additional utilities and core concepts
# Note that this contains both what you need to compile Actors and what you need to compile a Driver
add_library(gennylib SHARED
    src/version.cpp
    src/context.cpp
    src/Orchestrator.cpp
    src/PoolFactory.cpp
    )

# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(gennylib
    PRIVATE
        src
)

# Link in the sublibraries into the main library
target_link_libraries(gennylib
    PUBLIC
        gennylib_cast
        gennylib_core
)
if(ENABLE_JASPER)
    target_link_libraries(gennylib
        PRIVATE
            jasperlib
    )
endif()

# Link in OpenSSL
target_link_libraries(gennylib
    PUBLIC
        metrics
        OpenSSL::Crypto
        OpenSSL::SSL
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS  gennylib gennylib_cast gennylib_core
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp)

add_executable(test_gennylib
    test/Cast_test.cpp
    test/context_test.cpp
    test/conventions_test.cpp
    test/ExecutionStrategy_test.cpp
    test/PoolFactory_test.cpp
    test/PhaseLoop_test.cpp
    test/version_test.cpp
    test/orchestrator_test.cpp
)
target_link_libraries(test_gennylib testlib)

add_executable(benchmark_gennylib
    test/PhaseLoop_benchmark.cpp
    test/RateLimiter_benchmark.cpp
    test/orchestrator_benchmark.cpp
)

target_link_libraries(benchmark_gennylib testlib)


ParseAndAddCatchTests(test_gennylib)

ParseAndAddCatchTests(benchmark_gennylib)
