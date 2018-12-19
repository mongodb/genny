# For more info on object libraries,
# see https://cmake.org/cmake/help/v3.5/command/add_library.html#object-libraries
add_library(catch2 OBJECT ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp)
target_include_directories(catch2
    PUBLIC
        ${THIRD_PARTY_SOURCE_DIR}/catch2/include
)

