#!/usr/bin/env bash

pushd build >/dev/null
    cmake ..
    make -j8
    make
popd >/dev/null

