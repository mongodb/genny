# <yaml-cpp>
set(YAML_CPP_BUILD_TESTS OFF)
add_subdirectory(yaml-cpp)
# </yaml-cpp>

# <Catch2>
add_subdirectory(catch2)
# </Catch2>

# <Mongo Driver>
#find_package(libmongocxx REQUIRED)
# </Mongo Driver>

# <gRPC>
# Try to prefer our local openssl if it's there
set(OPENSSL_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openssl")
if(EXISTS "${OPENSSL_LOCAL_DIR}")
    set(OPENSSL_ROOT_DIR "${OPENSSL_LOCAL_DIR}" CACHE PATH "")
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

# Add our third_party path to the potential places to find gRPCConfig.cmake
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/grpc")
#find_package(gRPC REQUIRED)
# </gRPC>

# <Jasper>
#add_subdirectory(jasper)
# </Jasper>

# These libraries are almost ubiquitously required in genny
# The AFTER tag means that our headers are considered before headers in the same folder as these
# dependencies
include_directories(AFTER
    ${LIBMONGOCXX_INCLUDE_DIRS}
)
link_libraries(
    ${CMAKE_THREAD_LIBS_INIT}
    Boost:boost
    ${LIBMONGOCXX_LIBRARIES}
)
