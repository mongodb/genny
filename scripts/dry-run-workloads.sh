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

set -euo pipefail
set -x

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

CMD=(
    "${GENNY:-genny}"
    --dry-run
)

# Find all of the workload yamls at the specified directory
WORKLOAD_DIR="${1:-${ROOT_DIR}/src/workloads}"

WORKLOAD_LIST="${BUILD_DIR}/workload-list.txt"
if [ -e "${WORKLOAD_LIST}" ]; then
    rm "${WORKLOAD_LIST}"
fi

find "${WORKLOAD_DIR}" -name "*.yml" -print >"${WORKLOAD_LIST}"

# Dry run each file
while read -r FILE; do
    if [[ "${OSTYPE}" == "darwin"* ]] && [[ "${FILE}" == *"AuthNInsert"* ]]; then
      echo "TIG-1435 skipping dry run AuthNInsert.yml on macOS"
      continue;
    fi;
    1>&2 echo "-- Testing workload at '${FILE}' via --dry-run..."
    "${CMD[@]}" "${FILE}"
done <"${WORKLOAD_LIST}"

