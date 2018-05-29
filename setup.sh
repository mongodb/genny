#!/usr/bin/env bash

xcode-select --install

brew install cmake
brew install wget
brew install icu4c
brew install boost --build-from-source --include-test --with-icu4c
