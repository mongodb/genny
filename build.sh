#!/usr/bin/env bash

set -eou pipefail

docker build -t squeeze_dev_image .
docker run --rm -v "$PWD":/squeeze -it squeeze_dev_image bash
