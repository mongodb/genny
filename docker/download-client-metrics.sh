#!/bin/bash

set -euo pipefail

# The following variables can be overridden by the environment to change script
# behavior. Anything set in the environment can be overridden by options
# parsed by the script later on.

# The directory to use for downloads.
: "${OUTPUT_DIR:=containers}"


# The container name root.
: "${CONTAINER:=docker_genny}"

# Whether to keep any downloaded files across executions. This can be useful if
# you are having network problems or would otherwise like to cache the
# downloaded tarball URL.
: "${KEEP_FILES:=0}"

cleanup() {
    local exit_code=$1

    # Untrap so this function doesn't execute twice
    trap - INT TERM HUP QUIT EXIT

    # echo -e "\nCleaning up..."
    # if [[ -n ${OUTPUT_DIR} ]]; then
    #     if (( ! KEEP_FILES )); then
    #         rm -f "${OUTPUT_DIR}/${CONTAINER}*"
    #     fi
    # fi

    if (( exit_code != 0 )); then
        echo "Please report any errors in #sdp-build-help for further assistance." >&2
    fi

    echo
    exit "${exit_code}"
}

show_usage() {
    cat <<EOF

download-client-metrics.sh: Get the dock genny client workload data

This script gets the latest genny client workload data
and copies it to:

    ${OUTPUT_DIR}

Usage:
    download-client-metrics.sh [-h]
    download-client-metrics.sh [-k] [-o OUTPUT_DIR]  [-c CONTAINER]

Options:
    -h, --help           Show this help output
    -k, --keep-download  Keep old files on script exit (default: false)
    -o, --output-dir     Download workload data to directory OUTPUT_DIR  (default: containers)
    -c, --container      The docker client name roots (default: docker_genny)

EOF
}

while getopts ":hkoc:h:k:o:c:-:" opt; do
    case "${opt}" in
        -)
            case "${OPTARG}" in
                help)
                    show_usage
                    exit 0
                    ;;
                keep-download)
                    KEEP_FILES=1
                    RM_DOWNLOAD_DIR=0
                    ;;
                output-dir)
                    OUTPUT_DIR="${!OPTIND}"; OPTIND=$(( OPTIND + 1 ))
                    ;;
                output-dir=*)
                    OUTPUT_DIR="${OPTARG#*=}"
                    OUTPUT_DIR="${OUTPUT_DIR/#\~/${HOME}}"
                    ;;
                container)
                    CONTAINER="${!OPTIND}"; OPTIND=$(( OPTIND + 1 ))
                    ;;
                container=*)
                    CONTAINER="${OPTARG#*=}"
                    ;;
                *)
                    show_usage >&2
                    exit 1
                    ;;
            esac
            ;;
        h)
            show_usage
            exit 0
            ;;
        k)  KEEP_FILES=1
            RM_DOWNLOAD_DIR=0
            ;;
        o)
            OUTPUT_DIR="${OPTARG}"
            ;;
        c)
            CONTAINER="${OPTARG}"
            ;;
        \?)
            show_usage >&2
            exit 1
            ;;
        *)
            show_usage >&2
            exit 1
            ;;
    esac
done
shift $(( OPTIND - 1 ))


# From now on, we will clean up any files and directories we created on exit
trap 'cleanup "$?"' INT TERM HUP QUIT EXIT


#
# Download all genny client container build/WorkloadOutput
#
get_workload_output() {
    local container_dir=$1
    local container_name=$2
    local keep_files=${3:-0}

    if (( ! keep_files )); then
        rm -rf "${container_dir}"
    fi
    mkdir ${container_dir}
    for container_id in $(docker container ls  -a | grep ${container_name} | awk '{print $NF "," $1}'  | sort -V -k1,1 -t ',') ;do
        name=${container_id//,*}
        cid=${container_id##*,}
        printf "% 20s\n"  $namels 
        docker cp ${cid}:/home/genny/build/WorkloadOutput ${container_dir}/${name}
    done
}

echo "Downloading ${CONTAINER} to ${OUTPUT_DIR} ..."
get_workload_output ${OUTPUT_DIR}  ${CONTAINER} ${KEEP_FILES}

