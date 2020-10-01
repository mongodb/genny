#!/bin/bash

# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -o errexit
set -o pipefail
set -o nounset

pushd "$(dirname "$0")" >/dev/null
    LAMP_VENV_DIR="$(pwd -P)"
popd > /dev/null

source "$LAMP_VENV_DIR/env.sh"


ROOT_DIR="$(cd "${LAMP_VENV_DIR}/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/build/mongo"
VENV_DIR="${ROOT_DIR}/build/venv"

export PATH="/opt/mongodbtoolchain/v3/bin:$PATH"

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone git@github.com:mongodb/mongo.git "${SOURCE_DIR}"
fi

(
    cd "${SOURCE_DIR}"
    # See comment in evergreen.yml - mongodb_archive_url
    git checkout 6050e97eafbb6aec02d783f8ec3acea2c612b924
)

python3 -m pip install -r "${SOURCE_DIR}/etc/pip/evgtest-requirements.txt"
