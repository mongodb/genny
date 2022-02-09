add_executable(http-example main.cpp)

find_package(Boost 1.66 REQUIRED COMPONENTS
               date_time regex)
find_package(OpenSSL)
find_package(Threads)

include_directories( ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${cppcodec_INCLUDE_DIRS}
)

if(WIN32 AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.0.23026.0))
    target_compile_options(http-example PRIVATE "/bigobj")
endif()

target_link_libraries(http-example PRIVATE
    simple-beast-client
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
)
