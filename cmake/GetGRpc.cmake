## gRPC is not always available.
## We use it if it's there, and provide a way to build it if it is not.

find_package(gRPC CONFIG)

# Inspired by https://github.com/grpc/grpc/blob/v1.17.0/examples/cpp/helloworld/cmake_externalproject/CMakeLists.txt
include(ExternalProject)

## Install c-ares
set(cares_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
)

# Only build static libraries
list(APPEND cares_ARGS
    -DCARES_SHARED:BOOL=OFF
    -DCARES_STATIC:BOOL=ON
    -DCARES_STATIC_PIC:BOOL=ON
)

ExternalProject_Add(project_cares
    EXCLUDE_FROM_ALL ON

    GIT_REPOSITORY  git@github.com:c-ares/c-ares.git
    GIT_TAG         cares-1_13_0

    CMAKE_CACHE_ARGS    ${cares_ARGS}
)

## Install Protocol Buffers
set(protobuf_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
)

# No shared libs and no tests
list(APPEND protobuf_ARGS
    -Dprotobuf_BUILD_TESTS:BOOL=OFF
    -Dprotobuf_BUILD_SHARED_LIBS:BOOL=OFF
)

ExternalProject_Add(project_protobuf
    EXCLUDE_FROM_ALL ON

    GIT_REPOSITORY  git@github.com:protocolbuffers/protobuf.git
    GIT_TAG         v3.6.1

    SOURCE_SUBDIR   "cmake"

    CMAKE_CACHE_ARGS    ${protobuf_ARGS}
)

## Install gRPC
set(gRPC_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_PREFIX_PATH:PATH=${CMAKE_INSTALL_PREFIX}
    -DgRPC_BUILD_TESTS:BOOL=OFF
)

# Jump through all the hoops necessary to install gRPC 
list(APPEND gRPC_ARGS
    -DgRPC_INSTALL:BOOL=ON
    -DgRPC_PROTOBUF_PROVIDER:STRING=package
    -DgRPC_ZLIB_PROVIDER:STRING=package
    -DgRPC_CARES_PROVIDER:STRING=package
    -DgRPC_SSL_PROVIDER:STRING=package
)

# If we have a custom OpenSSL install, use it.
# (We probably do.)
if(OPENSSL_ROOT_DIR)
    list(APPEND gRPC_ARGS
        -DOPENSSL_ROOT_DIR:PATH=${OPENSSL_ROOT_DIR}
    )
endif()

ExternalProject_Add(project_grpc
    EXCLUDE_FROM_ALL ON
    DEPENDS
        project_cares
        project_protobuf

    GIT_REPOSITORY      git@github.com:grpc/grpc.git
    GIT_TAG             v1.17.0

    CMAKE_CACHE_ARGS    ${gRPC_ARGS}
)
