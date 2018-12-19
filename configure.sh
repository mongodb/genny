#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname ${BASH_SOURCE[0]})" && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"

# Add the build dir and move to it
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
BUILD_DIR="$(pwd)"

# Start building our cmake invocation
CMD=(cmake)

# Prefer the existing cache dir if we can
if [[ -d $BUILD_DIR/CMakeFiles ]]; then
    CMD+=("${BUILD_DIR}")
else
    CMD+=("${ROOT_DIR}")
fi

# Add our build type, this gets overridden by any later -DCMAKE_BUILD_TYPE args
CMD+=("-DCMAKE_BUILD_TYPE=Debug")

# Append on any arguments
if [[ ${#} -gt 0 ]]; then
    CMD+=("${@}")
fi

"${CMD[@]}"
