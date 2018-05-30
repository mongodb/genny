#!/usr/bin/env bash

set -eou pipefail

cd build
cmake ..
make
make test
