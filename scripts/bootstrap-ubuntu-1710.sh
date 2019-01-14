#!/usr/bin/env bash
set -eou pipefail

# This works for Ubuntu 17.10 only.

# system packages

sudo apt-get update
sudo apt-get upgrade

sudo apt-get install -y gcc make perl
sudo apt-get install -y \
    build-essential \
    git \
    cmake \
    software-properties-common \
    libboost-all-dev \
    autoconf \
    automake \
    libtool \
    curl \
    make \
    g++ \
    gcc-7 \
    unzip \
    pkg-config

# pre-built goodies updated as of 2019-01-14
if [ ! -d "genny-setup" ]; then
    wget https://s3-us-west-2.amazonaws.com/dsi-donot-remove/genny/genny-setup-ubuntu-17-10.tar.gz
    tar xzf genny-setup-ubuntu-17-10.tar.gz
fi
cd genny-setup

# mongo c driver
if [ ! -d "mongo-c-driver-1.13.0" ]; then
    wget https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz
    tar xzf mongo-c-driver-1.13.0.tar.gz
fi
pushd mongo-c-driver-1.13.0/build
cmake ..
make -j8
sudo make install
popd

# mongo c++ driver
if [ ! -d "mongo-cxx-driver" ]; then
    git clone https://github.com/mongodb/mongo-cxx-driver.git     --branch releases/stable --depth 1
fi
pushd mongo-cxx-driver/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
make clean
sudo make -j8 # may need sudo for this make
sudo make install
popd

# protobuf
if [ ! -d "protobuf" ]; then
    git clone https://github.com/protocolbuffers/protobuf.git
fi
pushd protobuf
git submodule update --init --recursive
./autogen.sh
./configure
make -j8
# make check
sudo make install
sudo ldconfig
popd


# grpc
# https://github.com/grpc/grpc/blob/master/BUILDING.md
if [ ! -d "grpc" ]; then
    git clone -b "$(curl -L https://grpc.io/release)" https://github.com/grpc/grpc
fi
pushd grpc
git submodule update --init
make -j8
sudo make install
popd

# cmake 10.13
if [ ! -e "cmake-3.13.2-Linux-x86_64.sh" ]; then
    wget https://github.com/Kitware/CMake/releases/download/v3.13.2/cmake-3.13.2-Linux-x86_64.sh
fi
chmod +x ./cmake-3.13.2-Linux-x86_64.sh
sudo ./cmake-3.13.2-Linux-x86_64.sh --prefix=/usr --skip-license

# clang-6.0
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-add-repository "deb http://apt.llvm.org/artful/ llvm-toolchain-artful-6.0 main"
sudo apt-get update
sudo apt-get install -y clang-6.0

# openssl 1.1
if [ ! -d openssl ]; then
    git clone git://git.openssl.org/openssl.git
fi
pushd openssl
CC=gcc-7 ./config --prefix=/usr/local/ssl --openssldir=/usr/local/ssl
make -j8
sudo make install
popd

# genny itself
if [ ! -d "genny" ]; then
    git clone https://github.com/10gen/genny.git
fi
pushd genny/build
cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl -DOPENSSL_LIBRARIES=/usr/local/ssl/lib \
    -DCMAKE_CXX_COMPILER=clang++-6.0 -DCMAKE_EXE_LINKER_FLAGS=-Wl,--no-as-needed ..
make -j8
popd

# run genny --list-actors
./genny/build/src/driver/genny --list-actors

