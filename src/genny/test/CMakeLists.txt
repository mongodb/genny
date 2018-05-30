set(test_files
    version_test.cpp
)

add_executable(
    test_genny
    ${test_files}
    ${THIRD_PARTY_SOURCE_DIR}/catch2/main.cpp
)
target_link_libraries(test_genny genny)

target_include_directories(test_genny
    PRIVATE ${THIRD_PARTY_SOURCE_DIR}/catch2/include
)

# TODO: `make test` fails unless you first do `make`
add_test(NAME    test_genny
         COMMAND test_genny)
