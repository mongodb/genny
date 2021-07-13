# Using Genny

This page describes how to run Genny and view its outputs. It also describes how
to add actors and workloads. For details on the development process, see [Developing Genny](./developing).

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
If you see errors like the following, try reducing the number of threads and duration in ./src/workloads/scale/InsertRemove.yml
```E0218 17:46:13.640106000 123145604628480 wakeup_fd_pipe.cc:40]         pipe creation failed (24): Too many open files```

Logging currently goes to stdout and metrics data (ftdc) is written to
`./build/CedarMetrics`.

## Viewing Metrics Data

After running, Genny will export data to the `build/CedarMetrics` directory as `.ftdc` files corresponding to the
actor and operation recorded.
When executed in CI after submitting a patch, this data will be visible in the Evergreen perf UI.

If running locally you can use the `export` command that Genny provides to export to CSV.
For example, to export the results of the Insert operation in the InsertRemove workload as CSV data:

```sh
./run-genny export build/CedarMetrics/InsertRemoveTest.Insert.ftdc -o insert.csv
```


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

