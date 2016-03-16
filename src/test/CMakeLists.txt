include_directories(
    ${CMAKE_SOURCE_DIR}/src/third_party/catch/include
)
include_directories(${CMAKE_SOURCE_DIR}/src/documents
    ${CMAKE_SOURCE_DIR}/src/values ${CMAKE_SOURCE_DIR}/src/nodes
    ${CMAKE_SOURCE_DIR}/src/stats)

add_executable(test_driver
${CMAKE_SOURCE_DIR}/src/third_party/catch/main.cpp documents.cpp
operations.cpp dot.cpp stats.cpp nodes.cpp values.cpp)

target_link_libraries(test_driver stats values workload nodes operations docs
parse ${Boost_LIBRARIES} ${LIBRARIES} ${YAMLCPP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_test(driver test_driver)
