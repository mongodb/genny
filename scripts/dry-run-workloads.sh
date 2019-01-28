#!/bin/bash

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
WORKLOAD_DIR="${1:-workloads}"
WORKLOAD_LIST="${BUILD_DIR}/workload-list.txt"
find "${WORKLOAD_DIR}" -name "*.yaml" -print >"${WORKLOAD_LIST}"

# Dry run each file
while read -r FILE; do
    1>&2 echo "-- Testing workload at '${FILE}' via --dry-run..."
    "${CMD[@]}" "${FILE}"
done <"${WORKLOAD_LIST}"
