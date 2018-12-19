#!/bin/bash

# Inspired by https://github.com/grpc/grpc/blob/master/test/distrib/cpp/run_distrib_test_cmake.sh

set -o errexit
set -o pipefail
set -o nounset

WORKING_DIR="$(dirname "${BASH_SOURCE[0]}")/src/third_party/grpc"

cloneGRpc() {
    rm -rf "${WORKING_DIR}" # Remove grpc and any cmake config copied up from runner
    git clone --recurse-submodules https://github.com/grpc/grpc.git "${WORKING_DIR}"

    cd "${WORKING_DIR}"
    WORKING_DIR="$(pwd)"

    git checkout b79462f186cc22550bc8d53a00ae751f50d194f5
}

buildProtobuf() {
    cd "${WORKING_DIR}/third_party/protobuf"

    BUILD_DIR="cmake/build"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    rm -rf *

    CMAKE_CMD=(
        cmake
        ..
    )

    CMAKE_CMD+=(
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
    )

    CMAKE_CMD+=(
        -DCMAKE_INSTALL_PREFIX="${WORKING_DIR}"
    )

    "${CMAKE_CMD[@]}"
    make install
}

buildCAres(){
    cd "${WORKING_DIR}/third_party/cares/cares"

    BUILD_DIR="cmake/build"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    rm -rf *

    CMAKE_CMD=(
        cmake
        ../..
    )

    # Flip c-ares to be static
    CMAKE_CMD+=(
        -DCARES_STATIC=ON
        -DCARES_SHARED=OFF
        -DCARES_STATIC_PIC=ON
    )

    CMAKE_CMD+=(
        -DCMAKE_INSTALL_PREFIX="${WORKING_DIR}"
    )

    "${CMAKE_CMD[@]}"
    make install
}

buildGRpc() {
    BUILD_DIR="${BUILD_DIR:-${WORKING_DIR}/cmake/build}"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    rm -rf *

    CMAKE_CMD=(
        cmake
        "${WORKING_DIR}"
    )

    # Install grpc into its own source dir for ease of access
    CMAKE_CMD+=(
      -DCMAKE_PREFIX_PATH="${WORKING_DIR}"
      -DCMAKE_INSTALL_PREFIX="${WORKING_DIR}"
    )

    # Make gRPC easier to install
    CMAKE_CMD+=(
        -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF
    )

    # Use system provided zlib and ssl
    CMAKE_CMD+=(
        -DgRPC_ZLIB_PROVIDER=package
        -DgRPC_SSL_PROVIDER=package
    )

    # Use compiled protobuf and c-ares
    CMAKE_CMD+=(
        -DgRPC_PROTOBUF_PROVIDER=package
        -DgRPC_CARES_PROVIDER=package
    )

    "${CMAKE_CMD[@]}"
    make install
}

cloneGRpc
(buildProtobuf)
(buildCAres)
(buildGRpc)
