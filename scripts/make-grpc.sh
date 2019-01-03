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
rm -rf "${BUILD_DIR:?}/"*
cd "${BUILD_DIR}"

# Find openssl at a set of locations we expect
export OPENSSL_ROOT_DIR=""
OPENSSL_DIRS=()

# Use the mongodbtoolchain if we have it
OPENSSL_DIRS+=(
    "/opt/mongodbtoolchain/v2"
)

# Check brew just in case
if command -V brew; then
    OPENSSL_DIRS+=(
        "$(brew --prefix openssl@1.1)"
        "$(brew --prefix openssl)"
    )
fi

# Otherwise, use the system dirs
OPENSSL_DIRS+=(
    "/usr/local"
    "/usr"
)

# Search through our dirs in order to find our openssl
for DIR in "${OPENSSL_DIRS[@]}"; do
    OPENSSL_EXE="${DIR}/bin/openssl"
    if [[ ! -f $OPENSSL_EXE ]]; then
        continue
    fi

    OPENSSL_ROOT_DIR="${DIR}"
    break
done

cmake "${CMAKE_ARGS[@]:-}" "${ROOT_DIR}"
make project_grpc
