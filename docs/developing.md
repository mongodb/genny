# Developing Genny

This page describes the details of developing in Genny: How to run tests, debug, sanitizers, etc.
For information on how to write actors and workloads, see [Using Genny](./using.md).

## Merge Queue Queue

Genny uses the [Github Merge Queue][merge queue]. When you have received approval
for your PR, simply click `Add to Merge Queue` and your PR will automatically
be tested and merged.

[merge queue]: https://docs.devprod.prod.corp.mongodb.com/evergreen/Project-Configuration/Merge-Queue


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

### Workload Documentation Generation

Markdown documentation is generated for the workload YAML files. When a workload is updated run the following command:

```sh
./run-genny generate-docs
```

An evergreen task verifies that docs have been updated for each commit. If this task fails you need to run the `generate-docs` command and include the updated documentation in your commit.

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

Each yaml configuration file will only run tests that are associated
with their specific tags. (Eg. `genny_sharded.yml` will only run
tests that have been tagged with the `[sharded]` tag.)

When creating a new Actor, `./run-genny create-new-actor` will generate a new test case
template to ensure the new Actor can run against different MongoDB topologies,
please update the template as needed so it uses the newly-created Actor.

Currently, the code to implement resmoke-test assumes that it will be run by
Evergreen. Therefore, to run resmoke-test locally, some modifications must be made.
The commands to make these modifications on MacOS and on Linux are detailed below.

These commands assume that `git status` last output line is
`nothing to commit, working tree clean`, and that the current working directory is
`genny/` i.e. the root of the repo.

Note: If you make any changes after having run `./run-genny install -d {your distro}`,
please run that command again to compile your changes.

1. `mkdir -p data/db`
2. `perl -i -pe 's!mongos,!mongos, "--dbpathPrefix", "data/db",!' src/lamplib/src/genny/tasks/run_tests.py`
3. `perl -i -pe 's!../../src/genny!../..!' src/resmokeconfig/{YML file you want to use}`
   ```sh
   # Example:
   perl -i -pe 's!../../src/genny!../..!' src/resmokeconfig/genny_sharded.yml
   ```
4. If using `genny_create_new_actor.yml`: `./run-genny create-new-actor SelfTestActor`.
   Fix the bug in the generated code, which is left as an exercise to ensure the reader understand the code.
	<details>
     <summary>But if you're short on time, click here for the cheat code</summary>
     <pre><code>perl -i -pe 's!count == 101!count == 100!' ./src/cast_core/test/SelfTestActor_test.cpp</code></pre>
	</details>
5. The default Actor name used by `genny_create_new_actor.yml` is `SelfTestActor`. If you use another Actor name, please replace `SelfTestActor` in `genny_create_new_actor.yml` with that name: `perl -i -pe 's!SelfTestActor!{your Actor name}!' src/resmokeconfig/genny_create_new_actor.yml`
6. `./run-genny clean`
7. `./run-genny install -d {your distro}`
8. `./run-genny cmake-test` (MacOS: Ignore the failed tests related to date & time,
   they're known to be flaky on MacOS ([EVG-19776](https://jira.mongodb.org/browse/EVG-19776)))
9. `./run-genny resmoke-test --suites {YML file}`
   ```sh
   # Example:
   ./run-genny resmoke-test --suites src/resmokeconfig/genny_sharded.yml
   ```
10. Cleanup: `git restore src/resmokeconfig/{YML file} src/lamplib/src/genny/tasks/run_tests.py && git clean -fd` (you can do `git clean -fdn` to preview what will be cleaned)

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

Genny has a verbose output mode:

```shell
./run-genny workload -v debug
```

Actor information can be printed for additional help, eg: when an
[actor is about to begin](https://github.com/mongodb/genny/blob/a3fd9cb1ee9954877281922cf5959b635d889599/src/cast_core/src/HelloWorld.cpp#L42-L45)
or when it has [ended](https://github.com/mongodb/genny/blob/a3fd9cb1ee9954877281922cf5959b635d889599/src/cast_core/src/HelloWorld.cpp#L58)

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


## Updating canned artifacts for re-smoke tests
The canned artifacts used for re-smoke tests can get outdated over time and will be required to update periodically.
Here is how to update the canned artifacts.

1. Download the latest artifacts and upload them to the S3 bucket.
	- Download the tgz package for a specific platform
  	- The downloaded package should be uploaded to S3 bucket - s3://dsi-donot-remove/compile_artifacts/. Make sure to make the uploaded file public.
2. Update [run_tests.py](https://github.com/mongodb/genny/blob/master/src/lamplib/src/genny/tasks/run_tests.py) with new S3 URLs
  	- CANNED_ARTIFACTS needs to be updated with the new S3 URL.

[fp]: https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives
[pi]: https://github.com/mongodb/genny/blob/762b08ee3b71184d5f521e82f7ce6d6eeb3c0cc9/src/workloads/docs/ParallelInsert.yml#L183-L189
