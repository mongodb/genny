project(gennylib VERSION 0.1 LANGUAGES CXX)

add_library(gennylib SHARED
    src/version.cpp
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


# Depend on a library that we defined in the top-level file
target_link_libraries(gennylib
    PUBLIC ${Boost_LIBRARIES}
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS  gennylib EXPORT GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This makes the project importable from the install directory
# Put config file in per-project dir (name MUST match), can also
# just go into 'cmake'.
install(EXPORT GennyLibraryConfig DESTINATION share/GennyLibrary/cmake)

# This makes the project importable from the build directory
export(TARGETS gennylib FILE GennyLibraryConfig.cmake)

add_executable(test_gennylib
    test/version_test.cpp
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

add_test(test_gennylib test_gennylib)
