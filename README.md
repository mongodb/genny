<img src="https://user-images.githubusercontent.com/119094/67700512-75458380-f984-11e9-9b81-668ea220b9fa.jpg" align="right" height="282" width="320">

Genny
=====

Genny is a workload-generator library and tool. It is implemented using
C++17.

As of 2019-11-11, Genny is now using the Evergreen Commit-Queue. When
you have received approval for your PR, simply comment `evergreen merge`
and your PR will automatically be tested and merged.

## Build and Install

Here're the steps to get Genny up and running locally:

1.  Install the development tools for your OS.

    -   Ubuntu 18.04: `sudo apt install build-essential`
    -   Red Hat/CentOS 7/Amazon Linux 2:
        `sudo yum groupinstall "Development Tools"`
    -   Arch: Everything should already be set up.
    -   macOS: `xcode-select --install`
    -   Windows: <https://visualstudio.microsoft.com/>

2.  Make sure you have a C++17 compatible compiler and Python 3.7 or newer.
    The ones from mongodbtoolchain v3 are safe bets if you're
    unsure. (mongodbtoolchain is internal to MongoDB).

3.  `./scripts/lamp [--linux-distro ubuntu1804/rhel7/amazon2/arch]` once
    you have Python 3.7+ installed.

    This command downloads Genny's toolchain, compiles Genny, creates its
    virtualenv, and
    installs Genny to `dist/`. You can rerun this command at any time to
    rebuild Genny. If your OS isn't the supported, please let us know in
    `#workload-generation` slack or on GitHub.

    Note that the `--linux-distro` argument is not needed on macOS.

    To avoid polluting your system with python changes, lamp creates a 
    virtualenv. Installing globally is not recommended, but may be done
    with the `-g` or `--run-global` option.

    You can also specify `--build-system make` if you prefer to build
    using `make` rather than `ninja`. Building using `make` may make
    some IDEs happier.

    If you get python errors from `lamp` such as this:

    > `TypeError: __init__() got an unexpected keyword argument 'capture_output'`

    ensure you have a modern version of python3. On a Mac, run
    `brew install python3` (assuming you have [homebrew
    installed](https://brew.sh/)) and then restart your shell.

## IDEs and Whatnot

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
-G some-build-system \
-DCMAKE_PREFIX_PATH=/data/mci/gennytoolchain/installed/x64-osx-shared \
-DCMAKE_TOOLCHAIN_FILE=/data/mci/gennytoolchain/scripts/buildsystems/vcpkg.cmake \
-DVCPKG_TARGET_TRIPLET=x64-osx-static
```

If you run `./scripts/lamp -b make` it should set up everything for you.
You just need to set the "Generation Path" to your `build` directory.


## Lint Workload YAML Files and Generate Test Reports

Please refer to `src/python/README.md` for more information on how to
lint YAML files and generating test reports.


## Running Genny Self-Tests

Genny has self-tests using Catch2. You can run them with the following command:

```sh
# Build Genny first: `./scripts/lamp [...]`
./scripts/lamp cmake-test
```

For more fine-tuned testing (eg. running a single test or excluding some) you
can manually invoke the test binaries:

```sh
# Build Genny first: `./scripts/lamp [...]`
./build/src/gennylib/gennylib_test "My testcase"
```

Read more about what parameters you can pass [here][catch2].

[catch2]: https://github.com/catchorg/Catch2/blob/v2.5.0/docs/command-line.md#specifying-which-tests-to-run

### Benchmark Tests

The above `cmake-test` line also runs so-called "benchmark" tests. They
can take a while to run and may fail occasionally on local developer
machines, especially if you have an IDE or web browser open while the
test runs.

If you want to run all the tests except perf tests you can manually
invoke the test binaries and exclude perf tests:

```sh
# Build Genny first: `./scripts/lamp [...]`
./build/src/gennylib/gennylib_test '~[benchmark]'
```

#### Actor Integration Tests

The Actor tests use resmoke to set up a real MongoDB cluster and execute
the test binary. The resmoke yaml config files that define the different
cluster configurations are defined in `src/resmokeconfig`.

resmoke.py can be run locally as follows:
```sh
# Set up virtualenv and install resmoke requirements if needed.
# From Genny's top-level directory.
python /path/to/resmoke.py --suite src/resmokeconfig/genny_standalone.yml
```

Each yaml configuration file will only run tests that are associated
with their specific tags. (Eg. `genny_standalone.yml` will only run
tests that have been tagged with the "[standalone]" tag.)

When creating a new Actor, `create-new-actor.sh` will generate a new test case
template to ensure the new Actor can run against different MongoDB topologies,
please update the template as needed so it uses the newly-created Actor.

## Patch-Testing and Evergreen

When restarting any of Genny's Evergreen self-tests, make sure you
restart *all* the tasks not just failed tasks. This is because Genny's
tasks rely on being run in dependency-order on the same machine.
Rescheduled tasks don't re-run dependent tasks.

## Debugging

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
./scripts/genny run                                                 \
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

### Workload Phase Configs

If your workload YAML files get too complex or if you would like to reuse parts
of a workload in another one, you can define one or more of your phases in a
separate YAML file.

The phase configurations live in `src/phases`. There's roughly one sub-directory
per theme, similar to how `src/workloads` is organized.

For an example external phase config, please see the
`ExternalPhaseConfig` section of the `HelloWorld.yml` workload.

A couple of tips on defining external phase configs:

1. Most existing workloads define their options at the `Actor` level, which is one
level above `Phases`. Because Genny recursively traverses up the YAML to find an
option, most of the options can be pushed down and defined at the phase level
instead. The notable exceptions are `Name`, `Type`, and `Threads`,
which must be defined on `Actor`.

2. `genny evaluate /path/to/your/workload` is your friend. `evaluate` prints out
the final YAML workload with all external phase definitions inlined.


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

Then, set the Genny module in DSI with your local Genny repo.

```sh
cd genny
evergreen set-module -m dsi -i <ID> # Use the build ID from the previous step.
```

In the browser window, select either `genny_patch_tasks` or `genny_auto_tasks`.
`genny_patch_tasks` will run any workloads that you have added or modified locally
(based on your git history). This is useful if you want to test only the workload(s)
you've been working on. 

`genny_auto_tasks` automatically runs workloads based on the evergreen environment
(variables from `bootstrap.yml` and `runtime.yml` in DSI) and an optional AutoRun
section in any workload, doing simple key-value matching between them. For example,
suppose we have a `test_workload.yml` file in a `workloads/*/` subdirectory,
containing the following AutoRun section:

```yaml
AutoRun:
  Requires:
    bootstrap:
      mongodb_setup: 
        - replica
        - single-replica
```

In this case, `test_workload` would be run whenever `bootstrap.yml`'s `mongodb_setup`
variable has a value of `replica` or `single-replica`. In practice, the workload
AutoRun sections are setup so that you can use `genny_auto_tasks` to run all relevant
workloads on a specific buildvariant.

Both `genny_patch_tasks` and `genny_auto_tasks` will compile mongodb and then run
the relevant workloads.

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

Run `./scripts/lamp --help` for information on what sanitizers there are.

To run with ASAN:

```sh
./scripts/lamp -b make -s asan
./scripts/lamp cmake-test
# Pick a workload YAML that uses your Actor below
ASAN_OPTIONS="detect_container_overflow=0" ./build/src/driver/genny run ./src/workloads/docs/HelloWorld.yml
```

The toolchain isn't instrumented with sanitizers, so you may get
[false-positives][fp] for Boost, hence the `ASAN_OPTIONS` flag.



[fp]: https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives


