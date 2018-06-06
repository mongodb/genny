Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

TLDR:

```sh
# on os-x:
xcode-select --install

brew install cmake
brew install icu4c
brew remove boost
brew install boost --build-from-source --include-test --with-icu4c --without-static

# All operating-systems:
cd build
cmake ..
make
make test
```

If not using OS X, ensure you have a recent C++ compiler and boost
installation. Then specify compiler path when invoking `cmake`:

```sh
cd build
cmake \
    -DCMAKE_CXX_COMPILER="g++-7" \
    ..
```

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug. We support using CLion and we
don't tolerate squiggles or import or build errors in CLion.

System Requirements
-------------------

The `setup.sh` script should ensure your system is ready to go.

-   CMake
-   clang 5+ (other compilers probably work but we only support clang)
-   Boost (dynamically-compiled)

System Setup
------------

-   ensure you have the latest version of XCode installed. Open XCode
    occasionally so it can install updates.
-   You will also need [Homebrew](https://brew.sh/) installed
-   Then just run `./setup.sh`
