#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

set -x

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
SCRIPTS_DIR="${ROOT_DIR}/scripts"
BUILD_DIR="${ROOT_DIR}/build"

cd "${BUILD_DIR}"
rm -rf ./*

"${CMAKE:-cmake}" "${ROOT_DIR}" \
    "${@:-}"

make
make install
