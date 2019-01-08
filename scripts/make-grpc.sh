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
if [[ ${CLEAN_BUILD:-yes} == yes ]]; then
    rm -rf "${BUILD_DIR:?}/"*
fi
cd "${BUILD_DIR}"

. "${SCRIPTS_DIR}/find-openssl.rc"

"${CMAKE:-cmake}" "${@:-}" "${ROOT_DIR}"
make project_grpc
