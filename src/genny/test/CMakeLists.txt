set(test_files
    version_test.cpp
)

include_directories(${THIRD_PARTY_SOURCE_DIR}/catch2/include)

add_executable(
    test_genny
    ${test_files}
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)
target_link_libraries(test_genny genny)
target_include_directories(test_genny PRIVATE ../..)

add_test(genny test_genny)
