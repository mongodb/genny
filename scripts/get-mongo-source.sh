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

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/build/mongo"
VENV_DIR="${ROOT_DIR}/build/venv"

export PATH="/opt/mongodbtoolchain/v3/bin:$PATH"

if [ ! -d "${SOURCE_DIR}" ]; then
    git clone git@github.com:mongodb/mongo.git "${SOURCE_DIR}"
fi

(
    cd "${SOURCE_DIR}"
    # See comment in evergreen.yml - mongodb_archive_url
    git checkout cda363f65bde8d93a7c679757efd3edf7c6e8ad9
)

virtualenv -p python3 "${VENV_DIR}"

export VIRTUAL_ENV_DISABLE_PROMPT="yes"
# shellcheck disable=SC1090
. "${VENV_DIR}/bin/activate"

python3 -m pip install -r "${SOURCE_DIR}/etc/pip/evgtest-requirements.txt"
