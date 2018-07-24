Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

**Quick-Start on OS X**:

```sh
# Download Xcode 10 Beta (or install apple clang 10+)
#    https://developer.apple.com/download/
#

sudo xcode-select -s /Applications/Xcode-beta.app/Contents/Developer

# install third-party packages and build-tools
brew install cmake
brew install icu4c
brew install mongo-cxx-driver
brew install yaml-cpp

cd build
cmake ..
make -j8
make test

# if you get boost errors:
brew remove boost
brew install boost --build-from-source \
    --include-test --with-icu4c --without-static
```

**Other Operating-Systems**:

If not using OS X, ensure you have a recent C++ compiler and boost
installation. You will also need packages installed corresponding to the
above `brew install` lines. Then specify compiler path when invoking
`cmake`.

E.g. for Ubuntu:

```sh
apt-get install -y \
    software-properties-common \
    clang-6.0 \
    make \
    libboost-all-dev \
    libyaml-cpp-dev

# install mongo C++ driver:
#   https://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/installation/

cd build
cmake \
    -DCMAKE_CXX_COMPILER=clang++-6.0 \
    -DCMAKE_CXX_FLAGS=-pthread \
    ..
```

**IDEs and Whatnot**

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

Code Style and Limitations
---------------------------

> Don't get cute.

Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for code-style etc.
Note that we're pretty vanilla.

Sanitizers
----------

Genny is periodically manually tested to be free of unknown sanitizer
errors. These are not currently run in a CI job. If you are adding
complicated code and are afraid of undefined behavior or data-races
etc, you can run the clang sanitizers yourself easily.

Running with TSAN:

    cd build
    FLAGS="-pthread -fsanitize=thread -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    make test
    ./src/driver/genny ../src/driver/test/Workload.yml

Running with ASAN:

    cd build
    FLAGS="-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    make test
    ./src/driver/genny ../src/driver/test/Workload.yml

Running with ubsan

    cd build
    FLAGS="-pthread -fsanitize=undefined -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8
    make test
    ./src/driver/genny ../src/driver/test/Workload.yml
