# For more info on object libraries,
# see https://cmake.org/cmake/help/v3.5/command/add_library.html#object-libraries
add_library(catch2 OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_library(catch2_headers INTERFACE)
target_include_directories(catch2_headers
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(catch2 catch2_headers)

include(ParseAndAddCatchTests.cmake)
set(AdditionalCatchParameters --reporter junit --durations yes --rng-seed 12345 --success)
