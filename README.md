Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

TLDR:

```sh
# on os-x:
./setup.sh
cd build
cmake ..
make
make test
```

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug. We support using CLion and we
don't tolerate squiggles or import or build errors in CLion.

Ubuntu support is TODO as of 2018-05-30 but should work without much
headache.

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
