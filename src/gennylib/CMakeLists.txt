# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(gennylib VERSION 0.0.1 LANGUAGES CXX)

# Pacify the CLion
file(GLOB_RECURSE HEADERS_FOR_CLION
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     CONFIGURE_DEPENDS
     include/*.hpp include/*.hh src/*.hh)
add_library(gennylib_headers SHARED
    EXCLUDE_FROM_ALL
    src/version.cpp
    ${HEADERS_FOR_CLION}
)

# Eventually will move gennylib/actors into its own lib so keep those files together
add_library(gennylib SHARED
    src/version.cpp
    src/context.cpp
    src/Actor.cpp
    src/ActorProducer.cpp
    src/Cast.cpp
    src/ExecutionStrategy.cpp
    src/Orchestrator.cpp
    src/PoolFactory.cpp

    src/value_generators/parser.cpp
    src/value_generators/value_generators.cpp
    src/value_generators/generators-private.cpp

)

add_subdirectory(jasper)

# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(gennylib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${Boost_INCLUDE_DIR}
        ${YAML_CPP_INCLUDE_DIR}
        ${LIBMONGOCXX_INCLUDE_DIRS}
    PRIVATE
        jasper
        src
        src/value_generators
)


# Depend on libraries that we defined in the top-level CMakeLists.txt file
target_link_libraries(gennylib
    PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        ${LIBMONGOCXX_LIBRARIES}
    PRIVATE
        jasperlib
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS  gennylib EXPORT GennyLibraryConfig
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
    test/PoolFactory_test.cpp
    test/PhaseLoop_test.cpp
    test/version_test.cpp
    test/metrics_test.cpp
    test/orchestrator_test.cpp
    test/PhaseLoop_perf_test.cpp
    test/document_test.cpp
    test/orchestrator_perf_test.cpp
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)

target_include_directories(test_gennylib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
        ${THIRD_PARTY_SOURCE_DIR}/catch2/include
)
target_link_libraries(test_gennylib
    PUBLIC ${Boost_LIBRARIES} gennylib
)

add_test(
    NAME    test_gennylib
    COMMAND test_gennylib
        -r junit
        -d yes
        --rng-seed 12345
        --success
        --out test_gennylib.junit.xml
)
