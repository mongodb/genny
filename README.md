<img src="https://user-images.githubusercontent.com/119094/67700512-75458380-f984-11e9-9b81-668ea220b9fa.jpg" align="right" height="282" width="320">

Genny
=====

Genny is a workload-generator with first-class support for
time-series data-collection of operations running against MongoDB.

Genny uses the [Evergreen Commit-Queue][cq]. When you have received approval
for your PR, simply comment `evergreen merge` and your PR will automatically
be tested and merged.

[cq]: https://github.com/evergreen-ci/evergreen/wiki/Commit-Queue

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

3.  `./run-genny install [--linux-distro ubuntu1804/rhel7/amazon2/arch]`

    This command downloads Genny's toolchain, compiles Genny, creates its
    virtualenv, and installs Genny to `dist/`. You can rerun this command
    at any time to rebuild Genny. If your OS isn't the supported, please let us know in
    `#workload-generation` slack or on GitHub.

    Note that the `--linux-distro` argument is not needed on macOS.

    You can also specify `--build-system make` if you prefer to build
    using `make` rather than `ninja`. Building using `make` may make
    some IDEs happier.

    If you get python errors, ensure you have a modern version of python3.
    On a Mac, run `brew install python3` (assuming you have [homebrew installed](https://brew.sh/))
    and then restart your shell.

## IDEs and Whatnot

We follow CMake and C++17 best-practices, so anything that doesn't work
via "normal means" is probably a bug.

We support using CLion and any conventional editors or IDEs (VSCode,
emacs, vim, etc.). Before doing anything cute (see
[CONTRIBUTING.md](./CONTRIBUTING.md)), please do due-diligence to ensure
it's not going to make common editing environments go wonky.

If you're using CLion, make sure to set `CMake options`
(in settings/preferences) so it can find the toolchain.

The cmake command is printed when `run-genny install` runs, you can
copy and paste the options into Clion. The options
should look something like this:

```bash
-G some-build-system \
-DCMAKE_PREFIX_PATH=/data/mci/gennytoolchain/installed/x64-osx-shared \
-DCMAKE_TOOLCHAIN_FILE=/data/mci/gennytoolchain/scripts/buildsystems/vcpkg.cmake \
-DVCPKG_TARGET_TRIPLET=x64-osx-static
```

See the following images:

### CLion ToolChain Settings

![toolchain](images/toolchains.png?raw=true "Toolchains Settings")

### CLion CMake Settings

![CMake](images/cmake.png?raw=true "Cmake Settings")

If you run `./run-genny install -b make` it should set up everything for you.
You just need to set the "Generation Path" to your `build` directory.

### Automatically Running Poplar before genny_core launch in CLion

Create a file called poplar.sh with the following contents:

```bash
#!/bin/bash
pkill -9 curator # Be careful if there are more curator processes that should be kept. 
DATE_SUFFIX=$(date +%s)
mv  build/CedarMetrics  build/CedarMetrics-${DATE_SUFFIX} 2>/dev/null
mv build/WorkloadOutput-${DATE_SUFFIX} 2>/dev/null

# curator is installed by cmake.
build/curator/curator poplar grpc &

sleep 1
```

Next create an external tool for poplar in CLion:

![poplar](images/poplar.png?raw=true "Poplar External tool")

*Note*: the Working directory value is required. 

Finally the external poplar tool to the CLion 'Before Launch' list:

![Debug](images/debug.png?raw=true "Debug Before Launch.")

## Running Genny Self-Tests

These self-tests are exercised in CI. For the exact invocations that the CI tests use, see evergreen.yml.

### Linters


Lint Python:

```sh
./run-genny lint-python # add --fix to fix any errors.
```

Lint Workload and other YAML:

```sh
./run-genny lint-yaml
```

### C++ Unit-Tests

```
./scripts/lamp self-test
```

For more fine-tuned testing (eg. running a single test or excluding some) you
can manually invoke the test binaries:

```sh
./build/src/gennylib/gennylib_test "My testcase"
```

Read more about what parameters you can pass [here][catch2].

[catch2]: https://github.com/catchorg/Catch2/blob/v2.5.0/docs/command-line.md#specifying-which-tests-to-run


### Benchmark Tests

The above `self-test` line also runs so-called "benchmark" tests. They
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

```sh
# Example:
./run-genny resmoke-test --suite src/resmokeconfig/genny_standalone.yml
```

Each yaml configuration file will only run tests that are associated
with their specific tags. (Eg. `genny_standalone.yml` will only run
tests that have been tagged with the `[standalone]` tag.)

When creating a new Actor, `./run-genny create-new-actor` will generate a new test case
template to ensure the new Actor can run against different MongoDB topologies,
please update the template as needed so it uses the newly-created Actor.


## Patch-Testing and Evergreen

When restarting any of Genny's Evergreen self-tests, make sure you
restart *all* the tasks not just failed tasks. This is because Genny's
tasks rely on being run in dependency-order on the same machine.
Rescheduled tasks don't re-run dependent tasks.

See below section on Patch-Testing Genny Changes with Sys-Perf / DSI
if you are writing a workload or making changes to more than just this repo.


## Debugging

IDEs can debug Genny if it is built with the `Debug` build type:

```sh
./run-genny install -- -DCMAKE_BUILD_TYPE=Debug
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
./run-genny workload -- run                                         \
    --workload-file       ./src/workloads/scale/InsertRemove.yml    \
    --mongo-uri           'mongodb://localhost:27017'
```

Logging currently goes to stdout and metrics data (ftdc) is written to
`./build/CedarMetrics`.

## Creating New Actors

To create a new Actor, run the following:

```sh
./run-genny create-new-actor NameOfYourNewActor
```

TODO: the docs output by create-new-actor are out of date.


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

1.  Most existing workloads define their options at the `Actor` level, which is one
    level above `Phases`. Because Genny recursively traverses up the YAML to find an
    option, most of the options can be pushed down and defined at the phase level
    instead. The notable exceptions are `Name`, `Type`, and `Threads`,
    which must be defined on `Actor`.

2.  `./run-genny workload -- evaluate ./src/workloads/docs/HelloWorld.yml` is your friend. `evaluate` prints out
    the final YAML workload with all external phase definitions inlined.


## Patch-Testing Genny Changes with Sys-Perf / DSI

Install the [evergreen command-line client](https://evergreen.mongodb.com/settings) and put it
in your PATH.

You will create an "empty" patch of the sys-perf project and then modify that patch to use
your custom version of genny with it.

Create a patch build from the mongo repository:

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
evergreen set-module -m genny -i <ID> # Use the build ID from the previous step.
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

NB:

1.  After the task runs you can call `set-module` again with more local changes from Genny or DSI.
    This lets you skip the 20 minute wait to recompile the server.
2.  Alternatively, you can follow the instructions at the top of the `system_perf.yml`
    file for information on how to skip compile.
3.  You can restart workloads from the Evergreen Web UI.


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

Run `./run-genny install --help` for information on what sanitizers there are.

To run with ASAN:

```sh
./run-genny install --build-system make --sanitizer asan
./run-genny self-test
# Pick a workload YAML that uses your Actor below
ASAN_OPTIONS="detect_container_overflow=0" ./run-genny workload -- run ./src/workloads/docs/HelloWorld.yml
```

The toolchain isn't instrumented with sanitizers, so you may get
[false-positives][fp] for Boost, hence the `ASAN_OPTIONS` flag.



[fp]: https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives


