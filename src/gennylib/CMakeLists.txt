# NB: this version number is duplicated in
# src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
project(gennylib VERSION 0.0.1 LANGUAGES CXX)

# Eventually will move gennylib/actors into its own lib so keep those files together
add_library(gennylib SHARED
    include/gennylib/version.hpp
                 src/version.cpp

        include/gennylib/context.hpp
        src/context.cpp

    include/gennylib/conventions.hpp

    include/gennylib/metrics.hpp

    include/gennylib/MetricsReporter.hpp

    include/gennylib/InvalidConfigurationException.hpp

    src/value_generators/parser.hh
    src/value_generators/parser.cpp
    include/gennylib/value_generators.hpp
    src/value_generators/generators-private.hh
    src/value_generators/value_generators.cpp
    src/value_generators/generators-private.cpp

    include/gennylib/Actor.hpp
    include/gennylib/ActorProducer.hpp
    include/gennylib/ActorVector.hpp

    include/gennylib/PhaseLoop.hpp

        include/gennylib/PhasedActor.hpp
                 src/PhasedActor.cpp

    include/gennylib/Orchestrator.hpp
                 src/Orchestrator.cpp

    include/gennylib/actors/HelloWorld.hpp
                 src/actors/HelloWorld.cpp

    include/gennylib/actors/Insert.hpp
                 src/actors/Insert.cpp
    include/gennylib/actors/InsertRemove.hpp
                 src/actors/InsertRemove.cpp

    # NextActorHere
)

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
        src
        src/value_generators
)


# Depend on libraries that we defined in the top-level CMakeLists.txt file
target_link_libraries(gennylib
    PUBLIC
        ${Boost_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        ${LIBMONGOCXX_LIBRARIES}
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS  gennylib EXPORT GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Below generates a cmake module so if you make install from this
# project you can find_package(GennyLib) in another cmake project.
#
# Please don't build any projects that link against gennylib outside of
# this repository please. Not yet. We need to think of a good ABI.
# We're only exposing external cmake bindings because it encourages
# best-practices and helps keep the codebase honest about what it
# exports and how it's used.
install(EXPORT GennyLibraryConfig DESTINATION share/GennyLibrary/cmake)

# This makes the project importable from the build directory
export(TARGETS gennylib FILE GennyLibraryConfig.cmake)

add_executable(test_gennylib
        test/context_test.cpp
    test/conventions_test.cpp
    test/PhaseLoop_test.cpp
    test/version_test.cpp
    test/metrics_test.cpp
    test/orchestrator_test.cpp
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
