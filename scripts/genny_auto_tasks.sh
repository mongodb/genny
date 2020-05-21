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

pushd "$(dirname "$0")" >/dev/null
    SCRIPTS_DIR="$(pwd -P)"
popd >/dev/null

source "$SCRIPTS_DIR/env.sh"

PYTHON_DIR="${SCRIPTS_DIR}/../src/python"
pip install -e "${PYTHON_DIR}"

genny-auto-tasks "$@"
