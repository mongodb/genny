#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/grpc"

# Hop into a fresh build dir
mkdir -p "${BUILD_DIR}"
rm -rf "${BUILD_DIR}/"*
cd "${BUILD_DIR}"

# Find openssl at a set of locations we expect
OPENSSL_ROOT_DIR=""
OPENSSL_DIRS=(
    "/opt/mongodbtoolchain/v2"
    "/usr/local/opt/openssl@1.1"
    "/usr/local/opt/openssl"
    "/usr/local"
    "/usr"
)
for DIR in "${OPENSSL_DIRS[@]}"; do
    OPENSSL_EXE="${DIR}/bin/openssl"
    if [[ ! -f $OPENSSL_EXE ]]; then
        continue
    fi

    OPENSSL_ROOT_DIR="-DOPENSSL_ROOT_DIR:PATH=${DIR}"
    break
done

# Use the mongodbtoolchain openssl to build and install grpc
cmake "${OPENSSL_ROOT_DIR}" "${CMAKE_ARGS[@]:-}" "${ROOT_DIR}"
make project_grpc
