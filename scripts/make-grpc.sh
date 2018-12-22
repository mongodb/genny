#!/bin/bash

# Inspired by https://github.com/grpc/grpc/blob/master/test/distrib/cpp/run_distrib_test_cmake.sh

set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"

GRPC_DIR="${ROOT_DIR}/src/third_party/grpc"
GRPC_SRC_DIR="${GRPC_DIR}/src"
GRPC_BUILD_DIR="${GRPC_DIR}/build"

cleanGRpc() {
    rm -rf "${GRPC_DIR}" # Remove grpc and any cmake config copied up from runner
}

cloneGRpc() {
    git clone --recurse-submodules https://github.com/grpc/grpc.git "${GRPC_SRC_DIR}"

    cd "${GRPC_SRC_DIR}"

    git checkout b79462f186cc22550bc8d53a00ae751f50d194f5
}

buildProtobuf() {
    cd "${GRPC_SRC_DIR}/third_party/protobuf"

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
        -DCMAKE_INSTALL_PREFIX="${GRPC_SRC_DIR}"
    )

    "${CMAKE_CMD[@]}"
    make install
}

buildCAres(){
    cd "${GRPC_SRC_DIR}/third_party/cares/cares"

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
        -DCMAKE_INSTALL_PREFIX="${GRPC_SRC_DIR}"
    )

    "${CMAKE_CMD[@]}"
    make install
}

buildGRpc() {
    mkdir -p "${GRPC_BUILD_DIR}"
    cd "${GRPC_BUILD_DIR}"
    rm -rf *

    CMAKE_CMD=(
        cmake
        "${GRPC_SRC_DIR}"
    )

    # Install grpc into its own source dir for ease of access
    CMAKE_CMD+=(
      -DCMAKE_PREFIX_PATH="${GRPC_DIR}"
      -DCMAKE_INSTALL_PREFIX="${GRPC_DIR}"
    )

    # Make gRPC easier to install
    CMAKE_CMD+=(
        -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF
    )

    # Use gRPC build-external instances of these components
    # zLib is usually system provided
    # c-ares and protobuf are compiled here
    # OpenSSL may or may not be compiled here
    CMAKE_CMD+=(
        -DgRPC_ZLIB_PROVIDER=package
        -DgRPC_SSL_PROVIDER=package
        -DgRPC_PROTOBUF_PROVIDER=package
        -DgRPC_CARES_PROVIDER=package
    )

    # Use our custom static OpenSSL if it's there
    if [[ -n ${OPENSSL_DIR} && -d ${OPENSSL_DIR} ]]; then
        CMAKE_CMD+=(
            "-DOPENSSL_ROOT_DIR=${OPENSSL_DIR}"
        )
    fi

    "${CMAKE_CMD[@]}"
    make install
}

(cleanGRpc)
(cloneGRpc)
(buildProtobuf)
(buildCAres)
(buildGRpc)
