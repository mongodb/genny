Genny 🧞‍
========

Genny is a workload-generator library and tool. It is implemented using
C++17.

## Build and Install

Here're the steps to get genny up and running locally:

1. Install the development tools for your OS.

    - Ubuntu 18.04: `sudo apt install build-essential`
    - Red Hat/CentOS 7/Amazon Linux 2: `sudo yum groupinstall "Development Tools"`
    - Arch: Grab a beer. Everything should already be set up.
    - macOS: `xcode-select --install`
    - Windows: <https://visualstudio.microsoft.com/>

1.  Make sure you have a C++17 compatible compiler and Python 3.
    The ones from mongodbtoolchain are safe bets if you're unsure.
    (mongodbtoolchain is internal to MongoDB).

1. `./scripts/lamp [--linux-distro ubuntu1804/rhel7/amazon2/arch]`

    This command downloads genny's toolchain, compiles genny and
    installs genny to `dist/`. You can rerun this command at any time
    to rebuild genny. If your OS isn't the supported, please let us
    know in \#workload-generation or on GitHub.

    Note that the `--linux-distro` argument is not needed on macOS.

### IDEs and Whatnot

We follow CMake and C++17 best-practices so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

If you're using CLion, make sure to set `CMake options`
(in settings/preferences) so it can find the toolchain.

The cmake command is printed when `lamp` runs, you can
copy and paste the options into Clion. The options
should look something like this:

```bash
-G Ninja \
-DCMAKE_PREFIX_PATH=/data/mci/gennytoolchain/installed/x64-osx-shared \
-DCMAKE_TOOLCHAIN_FILE=/data/mci/gennytoolchain/scripts/buildsystems/vcpkg.cmake \
-DVCPKG_TARGET_TRIPLET=x64-osx-static
```

Replace `osx` with `linux` if you're on Linux and remove `-G Ninja` if you
don't have `ninja` installed locally.

## Running Genny Self-Tests

Genny has self-tests using Catch2. You can run them with the following command:

```sh
# Build genny first: `./scripts/lamp [...]`
./scripts/lamp cmake-test
```

### Benchmark Tests

The above `cmake-test` line also runs so-called "benchmark" tests. They
can take a while to run and may fail occasionally on local developer
machines, especially if you have an IDE or web browser open while the
test runs.

If you want to run all the tests except perf tests you can manually
invoke the test binaries and exclude perf tests:

```sh
# Build genny first: `./scripts/lamp [...]`
./build/src/gennylib/gennylib_test '~[benchmark]'
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

### Debugging

IDEs can debug Genny if it is built with the `Debug` build type:

```sh
./scripts/lamp -DCMAKE_BUILD_TYPE=Debug
```

## Running Genny Workloads

First install mongodb and start a mongod process:

```sh
brew install mongodb
mongod --dbpath=/tmp
```

Then build Genny (see [above](#build-and-install) for details):

And then run a workload:

```sh
./build/src/driver/genny                                            \
    --workload-file       ./src/workloads/scale/InsertRemove.yml    \
    --metrics-format      csv                                       \
    --metrics-output-file build/genny-metrics.csv                   \
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

## Workload YAMLs

Workload YAMLs live in `src/workloads` and are organized by "theme". Theme
is a bit of an organic (contrived) concept so please reach out to us on Slack
or mention it in your PR if you're not sure which directory your workload
YAML belongs in.

All workload yamls must have an `Owners` field indicating which github team
should receive PRs for the YAML. The files must end with the `.yml` suffix.
Workload YAML itself is not currently linted but please try to make the files
look tidy.

## Patch-Testing Genny Changes with Sys-Perf / DSI

Install the [evergreen command-line client](https://evergreen.mongodb.com/settings) and put it
in your PATH.

Create a patch build from the mongo repository
```sh
cd mongo
evergreen patch -p sys-perf

```

You will see an output like this:

```
            ID : 5c533a2732f4174bbcb8bb2e
       Created : 35.656ms ago
    Description : <none>
         Build : https://evergreen.mongodb.com/patch/5c533a2732f4174bbcb8bb2e
      Finalized : No
```

Copy the value of the "ID" field and a browser window to the "Build" URL.

Then, set the genny module in DSI with your local genny repo.
```
cd genny
evergreen set-module -m dsi -i <ID> # Use the build ID from the previous step.
```

In the browser window, select the workloads you wish to run. Good
examples are Linux Standalone / `big_update` and Linux Standalone /
`insert_remove`.

The task will compile mongodb and will then run your workloads. Expect to
wait around 25 minutes.

NB: After the task runs you can call `set-module` again with more local changes.
You can restart the workloads from the Evergreen web UI.
This lets you skip the 20 minute wait to recompile the server.

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
    ./scripts/lamp -DCMAKE_CXX_FLAGS="$FLAGS"
    ./scripts/lamp cmake-test
    ./build/src/driver/genny ./workloads/docs/Workload.yml

Running with ASAN:

    FLAGS="-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"
    ./scripts/lamp -DCMAKE_CXX_FLAGS="$FLAGS"
    ./scripts/lamp cmake-test
    ./build/src/driver/genny ./workloads/docs/Workload.yml

Running with UBSAN

    FLAGS="-pthread -fsanitize=undefined -g -O1"
    ./scripts/lamp -DCMAKE_CXX_FLAGS="$FLAGS"
    ./scripts/lamp cmake-test
    ./build/src/driver/genny ./workloads/docs/Workload.yml
