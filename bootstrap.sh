#!/bin/bash

set -euo pipefail

export BUILD_DIR="${BUILD_DIR:-build}"

MAKE_ARGS=()
if [[ $# -eq 0 ]]; then
    MAKE_ARGS+=("all")
else
    MAKE_ARGS+=("${@}")
fi

./configure.sh
make -C build "${MAKE_ARGS[@]}"
