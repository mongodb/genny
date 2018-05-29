set(test_files
    version_test.cpp
)

include_directories(${THIRD_PARTY_SOURCE_DIR}/catch2/include)

add_executable(
    test_squeeze
    ${test_files}
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)
target_link_libraries(test_squeeze squeeze)
target_include_directories(test_squeeze PRIVATE ../..)

add_test(squeeze test_squeeze)
