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

# Concepts to do with Actors
add_library(gennylib_cast STATIC
    src/Actor.cpp
    src/ActorProducer.cpp
    src/Cast.cpp
    src/ExecutionStrategy.cpp
)

target_include_directories(gennylib_cast
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Concepts to do with generators
add_library(gennylib_generators STATIC
    src/value_generators/parser.cpp
    src/value_generators/value_generators.cpp
    src/value_generators/generators-private.cpp
)

target_include_directories(gennylib_generators
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src/value_generators
)

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
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
)

add_subdirectory(jasper)

# Link in the sublibraries into the main library
target_link_libraries(gennylib
    PUBLIC
        gennylib_cast
        gennylib_generators
    PRIVATE
        jasperlib
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS  gennylib gennylib_cast gennylib_generators
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp)

# Pull together our utils, header directories, and testing dependencies into one static target
add_library(gennylib_testutil STATIC
    test/ActorHelper.cpp
)
target_include_directories(gennylib_testutil
    PUBLIC
        src
        test
)
target_link_libraries(gennylib_testutil gennylib catch2)

add_executable(test_gennylib
    test/Cast_test.cpp
    test/context_test.cpp
    test/conventions_test.cpp
    test/PoolFactory_test.cpp
    test/PhaseLoop_test.cpp
    test/version_test.cpp
    test/metrics_test.cpp
    test/orchestrator_test.cpp
    test/document_test.cpp
    test/ActorHelper_test.cpp
)
target_link_libraries(test_gennylib gennylib_testutil)

add_test(
    NAME    test_gennylib
    COMMAND test_gennylib
        -r junit
        -d yes
        --rng-seed 12345
        --success
        --out test_gennylib.junit.xml
)

add_executable(benchmark_gennylib
    test/PhaseLoop_benchmark.cpp
    test/orchestrator_benchmark.cpp
)
target_link_libraries(benchmark_gennylib gennylib_testutil)

add_test(
    NAME    benchmark_gennylib
    COMMAND benchmark_gennylib
        -r junit
        -d yes
        --rng-seed 12345
        --success
        --out benchmark_gennylib.junit.xml
)
