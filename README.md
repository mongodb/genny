Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

TLDR:

```sh
# on os-x:
# Download Xcode 10 Beta (or install apple clang 10+)
#    https://developer.apple.com/download/
sudo xcode-select -s /Applications/Xcode-beta.app/Contents/Developer

# install third-party packages and build-tools
brew install cmake
brew install icu4c
brew install mongo-cxx-driver
brew install yaml-cpp

# if you get boost errors:
brew remove boost
brew install boost --build-from-source --include-test --with-icu4c --without-static


# All operating-systems:
cd build
cmake ..
make -j8
make test
```

If not using OS X, ensure you have a recent C++ compiler and boost
installation. Then specify compiler path when invoking `cmake`. E.g.
for Ubuntu:

```sh
cd build
cmake \
    -DCMAKE_CXX_COMPILER=clang++-6.0 \
    -DCMAKE_CXX_FLAGS=-pthread \
    ..
```

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug. We support using CLion and we
don't tolerate squiggles or import or build errors in CLion.

Sanitizers
----------

Running with TSAN:

    FLAGS="-pthread -fsanitize=thread -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    ./src/driver/genny ../src/driver/test/Workload.yml

Running with ASAN:

    FLAGS="-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    ./src/driver/genny ../src/driver/test/Workload.yml

Running with ubsan

    FLAGS="-pthread -fsanitize=undefined -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    ./src/driver/genny ../src/driver/test/Workload.yml
