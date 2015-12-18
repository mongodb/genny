include_directories(
    ${CMAKE_SOURCE_DIR}/src/third_party/catch/include
)
include_directories(${CMAKE_SOURCE_DIR}/src/documents ${CMAKE_SOURCE_DIR}/src/values ${CMAKE_SOURCE_DIR}/src/nodes ${CMAKE_SOURCE_DIR}/src/stats)

add_executable(test_driver
${CMAKE_SOURCE_DIR}/src/third_party/catch/main.cpp documents.cpp
operations.cpp dot.cpp stats.cpp nodes.cpp)

target_link_libraries(test_driver stats workload nodes operations docs
parse ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES} ${YAMLCPP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_test(driver test_driver)
