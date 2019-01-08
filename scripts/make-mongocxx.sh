#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
MONGOC_BUILD_DIR="${ROOT_DIR}/build/mongoc"
MONGOCXX_BUILD_DIR="${ROOT_DIR}/build/mongocxx"

. "${SCRIPTS_DIR}/find-openssl.rc"

if command -V xcrun; then
    xcrun -f --sdk macosx clang
fi

(
    git clone git@github.com:mongodb/mongo-c-driver.git "${MONGOC_BUILD_DIR}"

    cd "${MONGOC_BUILD_DIR}"

    git checkout r1.13

    ${CMAKE:-cmake} . \
        "-DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF" \
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON" \
        "-DENABLE_SSL:STRING=OPENSSL" \
        "${@:-}"
    make
    make install
)

(
    git clone git@github.com:bcaimano/mongo-cxx-driver.git "${MONGOCXX_BUILD_DIR}"
    cd "${MONGOCXX_BUILD_DIR}"

    git checkout ben_mixed_linking

    ${CMAKE:-cmake} . \
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON"         \
        "-DBSONCXX_POLY_USE_BOOST:BOOL=ON"                  \
        "-DUSE_STATIC_BOOST:BOOL=ON"                        \
        "-DBoost_NO_SYSTEM_PATHS:BOOL=ON"                   \
        "-DBUILD_SHARED_LIBS:BOOL=ON"                       \
        "-DBUILD_SHARED_AND_STATIC_LIBS:BOOL=ON"            \
        "-DBUILD_SHARED_LIBS_WITH_STATIC_MONGOC:BOOL=ON"    \
        "${@:-}"
    make
    make install
)
