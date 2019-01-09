#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/build/mongo"
VENV_DIR="${ROOT_DIR}/build/venv"

git clone git@github.com:mongodb/mongo.git "${SOURCE_DIR}"

(
    cd "${SOURCE_DIR}"
    git checkout 6734c12d17dd4c0e2738a47feb7114221d6ba66d
)

virtualenv -p python2 "${VENV_DIR}"

export VIRTUAL_ENV_DISABLE_PROMPT="yes"
# shellcheck disable=SC1090
. "${VENV_DIR}/bin/activate"

python -m pip install -r "${SOURCE_DIR}/etc/pip/evgtest-requirements.txt"
