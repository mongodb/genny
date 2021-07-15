# Developing Genny

This page describes the details of developing in Genny: How to run tests, debug, sanitizers, etc.
For information on how to write actors and workloads, see [Using Genny](./using.md).

## Commit Queue

Genny uses the [Evergreen Commit-Queue][cq]. When you have received approval
for your PR, simply comment `evergreen merge` and your PR will automatically
be tested and merged.

[cq]: https://github.com/evergreen-ci/evergreen/wiki/Commit-Queue


## Running Genny Self-Tests

These self-tests are exercised in CI. For the exact invocations that the CI tests use, see evergreen.yml.
All genny commands now use `./run-genny`. For a full list run `./run-genny --help`.

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

```sh
./run-genny cmake-test
```

### Python Unit-Tests
```sh
./run-genny self-test
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


### Actor Integration Tests

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

See [this page](./using.md)'s section on Patch-Testing Genny Changes with Sys-Perf / DSI
if you are writing a workload or making changes to more than just this repo.

## Debugging

IDEs can debug Genny if it is built with the `Debug` build type:

```sh
./run-genny install -- -DCMAKE_BUILD_TYPE=Debug
```

## Code Style and Limitations

> Don't get cute.

Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for code-style etc.
Note that we're pretty vanilla.

## Generating Doxygen Documentation

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

## Sanitizers

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
[pi]: https://github.com/mongodb/genny/blob/762b08ee3b71184d5f521e82f7ce6d6eeb3c0cc9/src/workloads/docs/ParallelInsert.yml#L183-L189


