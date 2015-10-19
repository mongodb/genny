include_directories(
    ${CMAKE_SOURCE_DIR}/src/third_party/catch/include
)
include_directories(${CMAKE_SOURCE_DIR}/src/documents)

add_executable(test_driver
${CMAKE_SOURCE_DIR}/src/third_party/catch/main.cpp documents.cpp)

target_link_libraries(test_driver docs parse ${Boost_LIBRARIES}
${LIBMONGOCXX_LIBRARIES} ${YAMLCPP_LIBRARIES}
${Boost_LOG_SETUP_LIBRARY} ${Boost_LOG_LIBRARY}
${Boost_FILESYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY}
${Boost_SYSTEM_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_test(driver test_driver)
