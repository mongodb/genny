#!/usr/bin/env bash

set -eou pipefail

if [ ! -d build ]; then
    mkdir build
fi

pushd build >/dev/null
    cmake ..
    make
    ./squeeze
popd >/dev/null
