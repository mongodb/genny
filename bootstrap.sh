#!/bin/bash

set -o errexit
set -o pipefail

function download_toolchain {
    OS_FAMILY=$(python -c "import platform;print(platform.system())")

    case $OS_FAMILY in
    "Darwin")
      TOOLCHAIN_URL=https://asdfsdf
      ;;
    "Linux")
      LINUX_DISTRO_ID=0

      while [[ "$LINUX_DISTRO_ID" -lt 1 ]] || [[ "$LINUX_DISTRO_ID" -gt 4 ]]; do

        echo "Which Linux distribution are you using? (Please enter the number before the name of the distro:"
        echo "If you can't find your distro, please let us know in #workload-generation"
        echo ""
        echo "1. Ubuntu 18.04"
        echo "2. Red Hat Enterprise Linux 7"
        echo "3. Arch Linux"
        echo "4. Amazon Linux 2"
        echo ""

        read -r LINUX_DISTRO_ID
      done

      case "$LINUX_DISTRO_ID" in
      "1")
        TOOLCHAIN_URL="https://1"
        ;;
      "2")
        TOOLCHAIN_URL="https://2"
        ;;
      "3")
        TOOLCHAIN_URL="https://3"
        ;;
      "4")
        TOOLCHAIN_URL="https://s3.amazonaws.com/genny/toolchain/gennytoolchain-Linux-4.9.76-38.79.amzn2.x86_64-x86_64-with-glibc2.2.5.tgz"
        ;;
      esac
      ;;
    *)
      echo "Unsupported Operating System $OS_FAMILY"
      exit 1
      ;;
    esac

    if [[ ! -f gennytoolchain.tgz ]]; then
        echo "Downloading toolchain archive..."
        curl -fLSs -o gennytoolchain.tgz "$TOOLCHAIN_URL"
    fi

    if [[ ! -d gennytoolchain ]]; then
        echo "Extracting toolchain archive..."
        tar -xf gennytoolchain.tgz gennytoolchain
    fi

    echo "Requesting sudo password to move toolchain to /opt/ if needed"
    sudo mkdir -p /opt
    sudo mv -f gennytoolchain /opt/
}

if [[ ! -d "/opt/gennytoolchain" ]]; then
    echo "Did not find /opt/gennytoolchain, downloading the toolchain for you"
    download_toolchain
fi

if [[ "$OS_FAMILY" == "Linux" ]] && [[ ! -d "/opt/mongodbtoolchain/v3/bin" ]]; then
  echo "WARNING: Did not find v3 mongodb toolchain on your system, please make sure you have a C++17 compatible compiler"
fi

# For mongodbtoolchain compiler (if there).
export PATH=/opt/mongodbtoolchain/v3/bin:$PATH

# For cmake and ctest
export PATH=/opt/gennytoolchain/downloads/tools/cmake-3.13.3-linux/cmake-3.13.3-Linux-x86_64/bin:$PATH

# For ninja
export PATH=/opt/gennytoolchain/downloads/tools/ninja-1.8.2-linux/:$PATH

# Positional arguments are treated as cmake flags.
cmake -DCMAKE_PREFIX_PATH=/opt/gennytoolchain/installed/x64-linux-shared -DCMAKE_TOOLCHAIN_FILE=/opt/gennytoolchain/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-static -B build -G Ninja "$@"

export NINJA_STATUS='[%f/%t (%p) %es] ' # make the ninja output even nicer
ninja -C build
ninja -C build install
