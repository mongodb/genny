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

See below section on Patch-Testing Genny Changes with Sys-Perf / DSI
if you are writing a workload or making changes to more than just this repo.


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

In the browser window, select either `schedule_patch_auto_tasks` or `schedule_variant_auto_tasks`.
`schedule_patch_auto_tasks` will run any workloads that you have added or modified locally
(based on your git history). This is useful if you want to test only the workload(s)
you've been working on. Note that `schedule_patch_auto_tasks` will only schedule a task for
a variant if the workload is compatible (see AutoRun below).

`schedule_variant_auto_tasks` automatically runs workloads based on the evergreen environment
(variables from `bootstrap.yml` and `runtime.yml` in DSI) and an optional AutoRun
section in any workload. The AutoRun section is a list of <When/ThenRun> blocks,
where if the When condition is met, tasks are scheduled with additional bootstrap
values from ThenRun. For example,
suppose we have a `test_workload.yml` file in a `workloads/*/` subdirectory,
containing the following AutoRun section:

```yaml
AutoRun:
  - When:
      mongodb_setup:
        $eq:
          - replica
          - replica-noflowcontrol
      branch_name:
        $neq:
          - v4.0
          - v4.2
    ThenRun:
      - infrastructure_provisioning: foo
      - infrastructure_provisioning: bar
      - arbitrary_key: baz
```

In this case, it looks in the `bootstrap.yml` of `test_workload`, checks if `mongodb_setup`
is either `replica` or `replica-noflowcontrol`, and also if `branch_name` is neither `v4.0` nor `v4.2`.
If both conditions are true, then we schedule several tasks. Let's say the workload name is
`DemoWorkload`, 3 tasks are scheduled - `demo_workload_foo`, `demo_workload_bar`, and `demo_workload_baz`.
The first task is passed in the bootstrap value `infrastructure_provisioning: foo`, the second
is passed in `infrastructure_provisioning: bar` and the third `arbitrary_key: baz`.

This is a more complex example of AutoRun. Here's a more simple one representing a more common usecase:

```yaml
AutoRun:
  - When:
      mongodb_setup:
        $eq: standalone
```

Let's say this is `DemoWorkload` again. In this case, if `mongodb_setup` is `standalone`
we schedule `demo_workload` with no additional params.

A few notes on the syntax:
- Supports multiple When/ThenRun blocks per AutoRun. Each are evaluated independently.
- When blocks can evaluate multiple conditions. All conditions must be true to schedule the task.
- When supports $eq and $neq. Both can accept either a scalar or list of values.
- For a list of values, $eq evaluates to true if it is equal to at least one.
- For a list of values, $neq evaluates to true if it is equal to none of the values.
- ThenRun blocks are optional.
    - **Most usecases do not need to use ThenRun**
    - If you do use ThenRun, please be judicious. If you have a task that is scheduled when
      mongodb_setup == replica, it would be confusing if mongodb_setup was overwritten to standalone.
      But it would be ok to overwrite mongodb_setup to replica-delay-mixed, as is done in the
      [ParallelWorkload][pi] workload.
- Each item in the ThenRun list can only support one {bootstrap_key: bootstrap_value} pair.
- If using ThenRun but you would also like to schedule a task without any bootstrap overrides,
  Add an extra pair to ThenRun with the original key/value, like done on line 189 [here][pi].
- If using ThenRun, the new task name becomes <taskname>_<bootstrap-value>. In the ParallelWorkload example,
  the task name becomes parallel_insert_replica_delay_mixed (name is automatically converted to snake_case).
  The bootstrap-key is not included in the name for the purpose of not changing existing names and
  thus deleting history. This may change after PM-2310.

NB on patch-testing:

1.  After the task runs you can call `set-module` again with more local changes from Genny or DSI.
    This lets you skip the 20 minute wait to recompile the server.
2.  Alternatively, you can follow the instructions at the top of the `system_perf.yml`
    file for information on how to skip compile.
3.  You can restart workloads from the Evergreen Web UI.

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


