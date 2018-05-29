#!/usr/bin/env bash

xcode-select --install

brew install cmake
brew install wget
brew install icu4c
brew install gdb
brew install gcc --with-nls --enable-static
cxx="$(brew --prefix gcc)/bin/g++-8" brew install boost --with-icu4c


# boost




# BOOST_PREFIX="$(dirname "$0")"/src/third_party/boost
# if [ ! -d "$BOOST_PREFIX" ]; then
#     mkdir "$BOOST_PREFIX"
# fi
#
# pushd build >/dev/null
#     if [ ! -d boost-install ]; then
#         mkdir boost-install
#     fi
#     pushd boost-install >/dev/null
#         if [ ! -e "boost_1_67_0.tar.gz" ]; then
#             wget "https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.gz"
#         fi
#         if [ ! -d "boost_1_67_0" ]; then
#             tar xzf "boost_1_67_0.tar.gz"
#         fi
#         pushd "boost_1_67_0" >/dev/null
#             ./bootstrap.sh --prefix="$BOOST_PREFIX"
#             # -d0 makes it qieter
#             ./b2 -d0 toolset=gcc install
#         popd >/dev/null
#     popd >/dev/null
# popd >/dev/null

