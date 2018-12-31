Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

## Build and Install

### macOS

1. [Download XCode 10](https://developer.apple.com/download/) (around 10GB) and install.
2. Drag `Xcode.app` into `Applications`. For some reason the installer may put it in `~/Downloads`.
3. Run the below shell (requires [`brew`](https://brew.sh/))

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

# install third-party packages and build-tools
brew install cmake
brew install icu4c
brew install mongo-cxx-driver
brew install grpc
brew install boost      \
    --build-from-source \
    --include-test      \
    --with-icu4c

cd build
cmake ..
make -j8
make test
```

If you get boost errors:

```sh
brew remove boost
brew install boost      \
    --build-from-source \
    --include-test      \
    --with-icu4c
```

Other errors, run `brew doctor`.

#### Notes for macOS Mojave

Mojave doesn't have `/usr/local` on system roots, so you need to set
environment variables for clang/ldd to find libraries provided by
homebrew.

Put the following in your shell profile (`~/.bashrc` or `~/.zshrc` etc):

```sh
export CPLUS_INCLUDE_PATH="$(brew --prefix)/include"
export LIBRARY_PATH="$(brew --prefix)/lib/:$LIBRARY_PATH"
```

This needs to be run before you run `cmake`. If you have already run
`cmake`, then `rm -rf build/*` after putting this in your profile
and restarting your shell.

TODO: TIG-1263 This is kind of a hack; using built-in package-location
mechanisms would avoid having to have OS-specific hacks like this.

### Linux Distributions

Have recent versions of non-vendored dependent packages in your system,
using the package manger. Generally this is:

- boost
- cmake (>=3.10.2)
- grpc (17)
- icu
- mongo-cxx-driver
- protobuf (3.6.1)

To build genny use the following commands:

```sh
cd build
cmake ..
make -j8
make test
```

You only need to run cmake once. Other useful targets include:

- all
- test_gennylib test_driver (builds tests)
- test (run's tests if they're built)

#### Ubuntu 18.04 LTS

For C++17 support you need at least Ubuntu 18.04. (Before you say "mongodbtoolchain", note that
it doesn't provide cmake.)

E.g. for Ubuntu:

```sh
apt install -y \
    build-essential \
    cmake \
    software-properties-common \
    clang-6.0 \ # optional
    libboost-all-dev
```

You also need `libgrpc++-dev` and `libprotobuf-dev`, but here genny is more picky about the
versions. They need to be 17 and 3.6.1 respectively. I ended up installing from source:

- protobuf: https://github.com/protocolbuffers/protobuf/blob/master/src/README.md
- grpc: https://github.com/grpc/grpc/blob/master/src/cpp/README.md

If you already installed the wrong versions with apt, you are better of uninstalling them:

```sh
apt remove libgrpc++-dev libprotobuf-dev
```

Finally, install mongo C++ driver from source too:
- https://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/installation/
- Note that you need to specifically tell it to install into `/usr/local`!

Now you can build genny as described in the previous section.

### IDEs and Whatnot

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
make -C "build" test_gennylib test_driver test
```

### Perf Tests

The above `make test` line also runs so-called "perf" tests. They can
take a while to run and may fail occasionally on local developer
machines, especially if you have an IDE or web browser open while the
test runs.

If you want to run all the tests except perf tests you can manually
invoke the test binaries and exclude perf tests:

```sh
make -C "build" benchmark_gennylib test
./build/src/gennylib/test_gennylib '~[benchmark]'
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

Then build Genny (see [above](#quick-start) for details):

And then run a workload:

```sh

./build/src/driver/genny                                      \
    --workload-file       src/driver/test/InsertRemove.yml \
    --metrics-format      csv                                 \
    --metrics-output-file build/genny-metrics.csv                 \
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

    FLAGS="-pthread -fsanitize=thread -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" -B "build" .
    make -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml

Running with ASAN:

    FLAGS="-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" -B "build" .
    make -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml

Running with UBSAN

    FLAGS="-pthread -fsanitize=undefined -g -O1"
    cmake -DCMAKE_CXX_FLAGS="$FLAGS" -B "build" .
    make -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml
