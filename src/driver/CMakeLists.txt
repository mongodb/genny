# Define an executable
add_executable(genny
    src/main.cpp
    src/DefaultDriver.cpp
)

# Define the libraries this project depends upon
target_link_libraries(genny gennylib gennylib_actors)

install(TARGETS  genny
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(test_driver
    # This is slightly non-optimal since we duplicate info about what sources make up the "driver" here
    # and in the add_executable line above. Maybe there should be a "driverlib" that we link against?
    # If the Driver(s) ever end up being more than one or two .cpp files, investigate that. For now, KISS.
    src/DefaultDriver.cpp
    test/Driver_test.cpp
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)

target_include_directories(test_driver
    PRIVATE
        src
        ${THIRD_PARTY_SOURCE_DIR}/catch2/include
)
target_link_libraries(test_driver
    PUBLIC ${Boost_LIBRARIES} gennylib gennylib_actors
)

add_test(
    NAME    test_driver
    COMMAND test_driver
        -r junit
        -d yes
        --rng-seed 12345
        --success
        --out test_gennylib.junit.xml
)

