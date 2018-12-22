#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"

OPENSSL_DIR="${ROOT_DIR}/src/third_party/openssl"
OPENSSL_SRC_DIR="${OPENSSL_DIR}/src"
OPENSSL_BUILD_DIR="${OPENSSL_DIR}/build"

cleanOpenSsl() {
    rm -rf "${OPENSSL_DIR}"
}

cloneOpenSsl() {
    git clone https://github.com/openssl/openssl "${OPENSSL_SRC_DIR}"

    cd "${OPENSSL_SRC_DIR}"

    git checkout OpenSSL_1_1_1
}

buildOpenSsl(){
    mkdir -p "${OPENSSL_BUILD_DIR}"
    cd "${OPENSSL_BUILD_DIR}"
    rm -rf *

    CONFIG_CMD=(
        "${OPENSSL_SRC_DIR}/config"
    )

    # Set our install prefix
    CONFIG_CMD+=(
        --prefix="${OPENSSL_DIR}"
        --openssldir="${OPENSSL_DIR}"
    )

    # Set our feature options
    CONFIG_CMD+=(
        no-shared
        no-ssl2
        no-ssl3
    )

    "${CONFIG_CMD[@]}"

    make -j4
    make -j4 install
}

(cleanOpenSsl)
(cloneOpenSsl)
(buildOpenSsl)
