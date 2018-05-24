include_directories(
    ${THIRD_PARTY_SOURCE_DIR}/catch2/include
)

add_executable(
    test_squeeze
    version_test.cpp
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)
target_link_libraries(test_squeeze lib_squeeze)
target_include_directories(test_squeeze PRIVATE ..)

add_test(squeeze test_squeeze)
