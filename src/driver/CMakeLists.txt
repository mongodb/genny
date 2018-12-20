project(driver VERSION 0.0.1 LANGUAGES CXX)

# Define an underlying core compilation unit
add_library(driver STATIC
    src/DefaultDriver.cpp
)
target_include_directories(driver
    PUBLIC
        src
)
target_link_libraries(driver cast_core gennylib)

# Make the genny driver executable
add_executable(genny src/main.cpp)
target_link_libraries(genny driver)
install(TARGETS  genny
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

# Define our test for the driver
add_executable(test_driver
    test/Driver_test.cpp
)
target_include_directories(test_driver
    PRIVATE
        src
)
target_link_libraries(test_driver driver catch2)

add_test(
    NAME    test_driver
    COMMAND test_driver
        -r junit
        -d yes
        --rng-seed 12345
        --success
        --out test_driver.junit.xml
)

# Install all the workload documents
file(GLOB WORKLOADS
     test/*.yml
     test/*.yaml
)
install(FILES ${WORKLOADS}
        DESTINATION etc/genny/workloads
)
