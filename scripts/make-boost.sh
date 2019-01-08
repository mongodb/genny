#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/boost"
INSTALL_DIR="${1-:boost}"

TAR_FILE="${ROOT_DIR}/boost.tar.bz2"

env

if command -V xcrun; then
    xcrun -f --sdk macosx clang
fi

BOOST_URL=https://dl.bintray.com/boostorg/release/1.65.1/source/boost_1_65_1.tar.bz2
curl "${BOOST_URL}" \
    --location \
    --output "${TAR_FILE}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

tar --strip-components=1 --bzip2 --extract --file "${TAR_FILE}"

export CFLAGS="-fPIC" CXXFLAGS="-fPIC"
./bootstrap.sh --prefix="${INSTALL_DIR}" \
    --without-libraries=python
./b2 cxxflags=-fPIC cflags=-fPIC install
