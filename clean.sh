#!/usr/bin/env bash

set -eou pipefail

cd "$(dirname $0)"
git clean -dfx build
