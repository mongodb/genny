Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

## Build and install

1. Install the development tools for your OS.

    - Ubuntu 16.04+: `sudo apt install build-essential`
    - Red Hat/CentOS 7+: `sudo yum groupinstall "Development Tools"`
    - Arch: Grab a beer. Everything should already be set up.
    - macOS: `xcode-select --install`
    - Windows: https://visualstudio.microsoft.com/

1. Download the MongoDB toolchain from [its Evergreen project](https://evergreen.mongodb.com/waterfall/toolchain-builder?task_filter=compile)
by clicking on the left-most green box corresponding to your OS and finding 
the link for `mongodbtoolchain.tar.gz`. This page is internal to MongoDB at the
moment. If you're unable to access it, you can just manually install any 
C++17 compatible compiler and skip this step.

1. Extract the MongoDB toolchain to "opt" if you have one: `[sudo] tar xvf mongodbtoolchain.tar.gz -C /opt/`

1. Fetch the latest genny toolchain, which is a [fork of Microsoft vcpkg](https://github.com/10gen/vcpkg)::
    - Ubuntu 17.10: [US east](https://s3.amazonaws.com/genny/toolchain/genny-vcpkg-prebuilt-ubuntu-17.10.tar.gz),
    [Sydney](https://s3-ap-southeast-2.amazonaws.com/genny-sydney/toolchain/genny-vcpkg-prebuilt-ubuntu-17.10.tar.gz)
    - TODO move this to its evergreen project
    
1. Extract the toolchain: `tar xvf genny-vcpkg-prebuilt-ubuntu-17.10.tar.gz`

1. `cd` into the toolchain directory `vcpkg` and run the toolchain integration script:
`./vcpkg integrate install`. This creates `.vcpkg` in your home directory. You can undo
the integration with `./vcpkg integrate remove` if needed.

1. `cd` into the this repo. Then:
```shell
CMAKE_PREFIX_PATH=/home/ubuntu/vcpkg/installed/x64-linux-shared \
/home/ubuntu/vcpkg/downloads/tools/cmake-3.13.3-linux/cmake-3.13.3-Linux-x86_64/bin/cmake \
-DCMAKE_TOOLCHAIN_FILE=/home/ubuntu/vcpkg/scripts/buildsystems/vcpkg.cmake 
-B build .
```
Replace `/home/ubuntu/vcpkg` with the path to your downloaded vcpkg if different.
You can also use your existing `cmake` instead of the one bundled with vcpkg if
you have the same or a newer version.

TODO: remove the need for CMAKE_PREFIX_PATH. It's temporarily needed to link the shared
mongo c++ driver library.

### IDEs and Whatnot

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

## Running Genny Self-Tests

Genny has self-tests using Catch2. You can run them with the following command:

```sh
make -j8 -C "build" test_gennylib test_driver test
```

### Benchmark Tests

The above `make test` line also runs so-called "benchmark" tests. They
can take a while to run and may fail occasionally on local developer
machines, especially if you have an IDE or web browser open while the
test runs.

If you want to run all the tests except perf tests you can manually
invoke the test binaries and exclude perf tests:

```sh
make -j8 -C "build" benchmark_gennylib test
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

## Running Genny Workloads

First install mongodb and start a mongod process:

```sh
brew install mongodb
mongod --dbpath=/tmp
```

Then build Genny (see [above](#build-and-install) for details):

And then run a workload:

```sh
./build/src/driver/genny                                    \
    --workload-file       src/driver/test/InsertRemove.yml  \
    --metrics-format      csv                               \
    --metrics-output-file build/genny-metrics.csv           \
    --mongo-uri           'mongodb://localhost:27017'
```

Logging currently goes to stdout, and time-series metrics data is
written to the file indicated by the `-o` flag (`./genny-metrics.csv`
in the above example).

Post-processing of metrics data is done by Python scripts in the
`src/python` directory. See [the README there](./src/python/README.md).

## Creating New Actors

To create a new Actor, run the following:

```sh
./scripts/create-new-actor.sh NameOfYourNewActor
```

## Code Style and Limitations

> Don't get cute.

Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for code-style etc.
Note that we're pretty vanilla.

### Generating Doxygen Documentation

We use Doxygen with a configuration that relies on llvm for its parsing
and graphviz for generating diagrams. As such you must compile Doxygen
with the appropriate support for these:

```sh
brew install graphviz
brew install doxygen --with-llvm --with-graphviz
```

Then generate and open Doxygen docs with the following:

```sh
doxygen .doxygen
open build/docs/html/index.html
```

### Sanitizers

Genny is periodically manually tested to be free of unknown sanitizer
errors. These are not currently run in a CI job. If you are adding
complicated code and are afraid of undefined behavior or data-races
etc, you can run the clang sanitizers yourself easily.

Running with TSAN:

    FLAGS="-pthread -fsanitize=thread -g -O1"
    cmake -E chdir "build" cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8 -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml

Running with ASAN:

    FLAGS="-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"
    cmake -E chdir "build" cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8 -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml

Running with UBSAN

    FLAGS="-pthread -fsanitize=undefined -g -O1"
    cmake -E chdir "build" cmake -DCMAKE_CXX_FLAGS="$FLAGS" ..
    make -j8 -C "build" test
    ./build/src/driver/genny src/driver/test/Workload.yml
