#!/usr/bin/env bash

xcode-select --install

brew install cmake
brew install icu4c
brew install boost --build-from-source --include-test --with-icu4c

if [ ! -e "src/third_party/catch2/include/catch.hpp" ]; then
    curl -fsS "https://raw.githubusercontent.com/CatchOrg/Catch2/master/single_include/catch.hpp" \
        > "src/third_party/catch2/include/catch.hpp"
fi