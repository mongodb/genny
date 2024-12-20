# Auto-generated Genny Tasks

!!! warning AutoRun in Genny is deprecated and no longer used

    AutoRun is no longer available in Genny. This documentation is
    kept for historical purposes only.


This folder contains the task defintions and buildvariant assignments for the
Genny workloads with `AutoRun` sections. They are kept for backwards 
compatibility purposes only for older versions of the mongo repository, 
and should not be modified.

Any modifications regarding which Genny tasks to run should happen in the Evergreen
Configuration found in the [DSI Repo](https://github.com/10gen/dsi/tree/master/evergreen/system_perf).

## Patch-testing a new Genny Workload

If you are patch testing a new test, you need to regenerate the files and run
the patch with `--include-modules` as follows:

```bash
# In the Genny repository
./run-genny auto-tasks-local

# In the mongo repository
evergreen patch -p sys-perf -u --include-modules
```

Make sure to specify the path to your local Genny repository when the second
command asks for it. Evergreen also caches this setting in your
`~/.evergreen.yml` config

## Adding a new Variant in DSI

If you are adding a new variant in DSI, you need to create a companion PR in
the Genny repo that updates the task definitions so that the Genny `AutoRun`
tasks are scheduled on it.

For this companion PR, run the following command:

```bash
# In the Genny repository
./run-genny auto-tasks-local
```

Then commit the changes to the auto-generated files in `evergreen/sys-perf`.

## Branching the Mongo Repository

If you're creating a new branch in the mongo repository, perform the following steps: 

1. Follow the [steps for branching](https://github.com/10gen/dsi/blob/master/evergreen/system_perf/README.md) in the DSI repository
2. Update the [auto_tasks_local.py](../../src/lamplib/src/genny/tasks/auto_tasks_local.py) PROJECT_FILES variable to include a reference to the newly created branch in the DSI repository
3. Run `./run-genny auto-tasks-local`
4. You should see a newly created folder in `evergreen/system_perf` for the new branch.

Please reach out in the `#ask-devprod-performance` channel if you run into problems or have any questions.