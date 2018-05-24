#!/usr/bin/env bash

set -eou pipefail

if [ ! -d docker-build ]; then
    mkdir docker-build
fi
pushd docker-build >/dev/null
    if [ ! -e cmake-3.10.1.tar.gz ]; then
        wget http://www.cmake.org/files/v3.10/cmake-3.10.1.tar.gz
    fi
    if [ ! -d cmake-3.10.1 ]; then
        tar -xzf cmake-3.10.1.tar.gz
    fi
    pushd cmake-3.10.1 >/dev/null
        ./bootstrap
        make -j8
        make install
    popd >/dev/null
popd >/dev/null
