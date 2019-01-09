#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

cd "${BUILD_DIR}"
rm -rf ./*

# shellcheck disable=SC1090
. "${SCRIPTS_DIR}/find-openssl.rc"

"${CMAKE:-cmake}" "${ROOT_DIR}" \
    "${@:-}"

make
make install
