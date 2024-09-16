# Auto-generated Genny Tasks

This folder contains the task defintions and buildvariant assignments for the
Genny workloads with `AutoRun` sections. These files are automatically
generated and should never be updated manually.

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
