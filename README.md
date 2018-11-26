Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

**Quick-Start on OS X**:

1. [Download XCode 10](https://developer.apple.com/download/) (around 10GB) and install.
2. Drag `Xcode.app` into `Applications`. For some reason the installer may put it in `~/Downloads`.
3. Run the below shell (requires [`brew`](https://brew.sh/))

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

# install third-party packages and build-tools
brew install cmake
brew install icu4c
brew install mongo-cxx-driver
brew install yaml-cpp
brew install grpc
brew install boost --build-from-source \
    --include-test --with-icu4c --without-static

cd build
cmake ..
make -j8
make test
```

If you get boost errors:

```sh
brew remove boost
brew install boost --build-from-source \
    --include-test --with-icu4c --without-static
```

Other errors, run `brew doctor`.


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
    libyaml-cpp-dev \
    libgrpc++-dev

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

Running Genny Self-Tests
------------------------

Genny has self-tests using Catch2. You can run them with the following command:

```sh
cd build
cmake ..
make .
make test
```

**Perf Tests**

The above `make test` line also runs so-called "perf" tests. They can
take a while to run and may fail occasionally on local developer
machines, especially if you have an IDE or web browser open while the
test runs.

If you want to run all the tests except perf tests you can manually
invoke the test binaries and exclude perf tests:

```sh
cd build
cmake ..
make .
./src/gennylib/test_gennylib '~[perf]'
```

Read more about specifying what tests to run [here][s].

[s]: https://github.com/catchorg/Catch2/blob/master/docs/command-line.md#specifying-which-tests-to-run

Running Genny Workloads
-----------------------

First install mongodb and start a mongod process:

```sh
brew install mongodb
mongod --dbpath=/tmp
```

Then build Genny (see above for more):

```sh
cd build
cmake ..
make -j8
make test
cd ..
```

And then run a workload:

```sh
cd build
./src/driver/genny                                            \
    --workload-file       ../src/driver/test/InsertRemove.yml \
    --metrics-format      csv                                 \
    --metrics-output-file ./genny-metrics.csv                 \
    --mongo-uri           'mongodb://localhost:27017'
```

Logging currently goes to stdout, and time-series metrics data is
written to the file indicated by the `-o` flag (`./genny-metrics.csv`
in the above example).

Post-processing of metrics data is done by Python scripts in the
`src/python` directory. See [the README there](./src/python/README.md).

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
