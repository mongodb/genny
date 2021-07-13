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

## Viewing Metrics Data

After running, Genny will export data to the `build/CedarMetrics` directory as `.ftdc` files corresponding to the
actor and operation recorded.
When executed in CI after submitting a patch, this data will be visible in the Evergreen perf UI.

If running locally you can use the `export` command that Genny provides to export to CSV.
For example, to export the results of the Insert operation in the InsertRemove workload as CSV data:

```sh
./run-genny export build/CedarMetrics/InsertRemoveTest.Insert.ftdc -o insert.csv
```

