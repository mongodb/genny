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

# This script wraps the genny-auto-tasks entry-point in src/python,
# running it inside a virtual environment with python3 and required dependencies.

set -eou pipefail

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
PYTHON_DIR="${ROOT_DIR}/src/python"
cd "${PYTHON_DIR}"

_pip() {
    /usr/bin/env pip "$@" --isolated -q -q
}

if [[ ! -d "venv" ]]; then
	python_path=/opt/mongodbtoolchain/v3/bin/python3
	if [[ ! -e "$python_path" ]]; then
		echo "${python_path} does not exist"
	    python_path="$(command -v python3)"
	fi

	echo "creating new env with python: ${python_path}"
	_pip install virtualenv
	/usr/bin/env virtualenv -q "venv" --python="${python_path}" >/dev/null
fi

set +u
	# shellcheck source=/dev/null
	source "venv/bin/activate"
set -u
_pip install -e "."

genny-auto-tasks "$@"
