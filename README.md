Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

### Quick-Start

#### macOS

1. [Download XCode 10](https://developer.apple.com/download/) (around 10GB) and install.
2. Drag `Xcode.app` into `Applications`. For some reason the installer may put it in `~/Downloads`.
3. Run the below shell (requires [`brew`](https://brew.sh/))

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

# install third-party packages and build-tools
brew install cmake
brew install icu4c
brew install openssl@1.1
brew install mongo-cxx-driver
brew install grpc
brew install boost      \
    --build-from-source \
    --include-test      \
    --with-icu4c

# install a local grpc for your genny dir
./scripts/make-grpc.sh

export OPENSSL_ROOT_DIR="$(brew --prefix openssl@1.1)"
cmake -B "build" .
make -C "build" genny
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

##### Notes for macOS Mojave

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

#### Linux Distributions

Have installations of non-vendored dependent packages in your system,
using the package manger. Generally this is:

- cmake
- boost
- mongo-cxx-driver
- grpc
- openssl
- icu

To to build genny use the following commands:

```sh
cmake -B "build" .
make -C "build" genny
```

You only need to run cmake once. Other useful targets include:

- all
- test_gennylib test_driver (builds tests)
- test (run's tests if they're built)

#### Other Operating Systems

If not using OS X, ensure you have a recent C++ compiler and boost
installation. You will also need packages installed corresponding to the
above `brew install` lines.

E.g. for Ubuntu:

```sh
apt-get install -y \
    software-properties-common \
    clang-6.0 \
    make \
    libssl-dev \
    libgrpc++-dev \
    libboost-all-dev

# install mongo C++ driver:
#   https://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/installation/
```

#### IDEs and Whatnot

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

### Running Genny Self-Tests

Genny has self-tests using Catch2. You can run them with the following command:

```sh
make -C "build" test_gennylib test_driver test
```

#### Perf Tests

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

#### Actor Integration Tests

The actor tests use resmoke to set up a real MongoDB cluster and execute
the test binary. The resmoke yaml config files that define the different
cluster configurations are defined in `src/resmokeconfig`.

resmoke.py can be run locally as follows:
```sh
# Set up virtualenv and install resmoke requirements if needed.
# From genny's top-level directory.
python /path/to/resmoke.py --suite src/resmokeconfig/genny_standalone.yml
```

Each yaml configuration file will only run tests that are associated
with their specific tags. (Eg. `genny_standalone.yml` will only run
tests that have been tagged with the "[standalone]" tag.)

When creating a new actor, `create-new-actor.sh` will generate a new test case
template to ensure the new actor can run against different MongoDB topologies,
please update the template as needed so it uses the newly created actor.

### Running Genny Workloads

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

### Code Style and Limitations

> Don't get cute.

Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for code-style etc.
Note that we're pretty vanilla.

### Sanitizers

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
