#!/usr/bin/env bash

set -eou pipefail

cd build
cmake ../src
make
make test
