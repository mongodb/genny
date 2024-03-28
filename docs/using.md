# Using Genny
[go/using-genny](http://go/using-genny)

## Table of Contents

1.  [Introduction](#orgf1f92f6)
2.  [Getting Started and Building](#org35e6dff)
3.  [Core Concepts](#org1140a6b)
    1.  [What is load generation?](#orgdcd1898)
	2.  [What other load generation tools are there?](#orgc7988ae)
    3.  [What is the system under test?](#orgc7904ae)
    4.  [What is a workload?](#org3610c67)
        1.  [How are workloads configured?](#orgdecc7ae)
        2.  [What is an Actor?](#org51d4d33)
        3.  [What is a phase?](#orgb655d69)
        4.  [How do I run a workload?](#org32b8ad3)
    5.  [Outputs](#orgec88ad4)
        1.  [Analyzing workload output locally](#analyzing-workload-output-locally)
    6.  [Workload Development](#org0e7c476)
4.  [Further Concepts](#org61c719c)
    1.  [Common Actors](#org78b250a)
    2.  [AutoRun](#org2b04b49)
        1.  [What is AutoRun?](#orgd0067d1)
        2.  [Configuring AutoRun](#orgbfb0d8e)
    3.  [Value Generators](#orgd89f221)
    4.  [Preprocessor](#org2078b23)
        1.  [LoadConfig](#orga6d35c7)
        2.  [ActorTemplate](#orga45b1d8)
        3.  [OnlyActiveInPhases](#orgf9c328f)
        4.  [FlattenOnce](#preprocessorflattenonce)
        5.  [PreprocessorFormatString](#preprocessorformatstring)
        6.  [NumExpr](#preprocessornumexpr)
        7.  [Defaults and Overrides](#org22b7a0f)
    5.  [Connecting to the Server](#orgd6b0450)
        1.  [Connection Strings and Pools](#orgd2659db)
        2.  [Multiple Connection Strings](#orga591018)
        3.  [Default](#org65830c2)
    6.  [Creating an Actor](#org7e6c6bd)
    7.  [Enabling Client-Side Field Level Encryption](#org627591d)
5.  [Pitfalls](#org3aaae9e)
    1.  [pipe creation failed (24): Too many open files](#orga7ab911)
    2.  [Actor integration tests fail locally](#orgb084b49)
    3.  [The Loader agent requires thread count set on both Actor and phase level](#org97681a9)


<a id="orgf1f92f6"></a>

# Introduction

Hello! These are the docs for Genny specifically. Genny is a workload-generator with first-class support for time-series data-collection of operations running against MongoDB. It is recommended for use in all MongoDB load generation. For an overall view of MongoDB's performance testing infrastructure, please look at the [Performance Tooling Docs](https://github.com/10gen/performance-tooling-docs) and the [architecture diagram](https://github.com/10gen/performance-tooling-docs/blob/main/perf_pipeline.md) that shows how Genny fits in the overall performance pipeline with other tools.

If you have any questions, please reach out to the DEVPROD team in our dedicated slack channel: [#ask-devprod-performance](https://mongodb.slack.com/archives/C01VD0LQZED). If you feel like these docs can be improved in any way, feel free to open a PR and assign someone from DEVPROD. No ticket necessary. This document is intended to be readable straight-through, in addition to serving as a reference on an as-needed basis. If there are any difficulties in flow or discoverability, please let us know.


<a id="org35e6dff"></a>

# Getting Started and Building

For build instructions, see the installation guide [here](setup.md).

To try launching Genny, navigate to the root of the Genny repo and run the following: 

```bash
./run-genny workload src/workloads/docs/HelloWorld.yml
```
	
You should see output similar to the following:

```
[curator] 2022/03/24 10:44:11 [p=info]: starting poplar gRPC service at 'localhost:2288'
[2022-03-24 10:44:12.376776] [0x000070000af63000] [info]    Hello Phase 0 🐳
[2022-03-24 10:44:12.376810] [0x000070000aee0000] [info]    Hello Phase 0 🐳
[2022-03-24 10:44:12.376823] [0x000070000af63000] [info]    Counter: 1
[2022-03-24 10:44:12.376833] [0x000070000aee0000] [info]    Counter: 2
[2022-03-24 10:44:12.376859] [0x000070000af63000] [info]    Hello Phase 0 🐳
[2022-03-24 10:44:12.376872] [0x000070000aee0000] [info]    Hello Phase 0 🐳
[2022-03-24 10:44:12.376877] [0x000070000af63000] [info]    Counter: 3
[2022-03-24 10:44:12.376888] [0x000070000aee0000] [info]    Counter: 4
[2022-03-24 10:44:12.376903] [0x000070000af63000] [info]    Hello Phase 0 🐳
.... (more lines of output) ....
[2022-03-24 10:44:56.942097] [0x0000700010152000] [info]    Hello Phase 2
[2022-03-24 10:44:56.942104] [0x00007000100cf000] [info]    Hello Phase 2
[2022-03-24 10:44:56.942112] [0x0000700010152000] [info]    Counter: 3369
[2022-03-24 10:44:56.942119] [0x00007000100cf000] [info]    Counter: 3370
[curator] 2022/03/24 10:44:58 [p=info]: poplar rpc service terminated
```


Note that the above test workload does not connect to a remote server, while most do. For more details, see [What is the System Under Test?](#orgc7904ae)

Whenever you have questions about the Genny CLI, you can always use the `-h` option for the top-level Genny CLI or any subcommands:

```bash
./run-genny -h # See all subcommands
./run-genny workload -h # See args and options for the workload subcommand
```


<a id="org1140a6b"></a>

# Core Concepts

This section introduces the core concepts used by Genny, the minimal required syntax for its inputs, and builds up a basic example.


<a id="orgdcd1898"></a>

## What is load generation?

Genny is a **load generation** tool. It is used as a client in [load tests](https://en.wikipedia.org/wiki/Load_testing) to perform a large amount of work against a system under test, measuring the client-side visible responses of said test subject. These measurements may represent things like latency, number of operations performed, how many errors occurred, etc. The procedure Genny uses to perform the work should be as deterministic and repeatable as possible. This procedure is called a **workload**.

Results of a load test can inform developers as to the performance of the test subject in stressful situations. How does a system handle increasing numbers of connections performing conflicting operations? How does a system handle a large number of users performing one operation, then simultaneously switching to another in an instant? How does one user initiating a long-running, expensive operation affect the latencies observed by other users? How does the system respond when put under load much higher than originally intended (stress testing)? All these situations can be simulated with Genny.

<a id="orgc7988ae"></a>

### What other load generation tools are there?
At MongoDB, we've historically used [benchrun-based](https://github.com/10gen/workloads) workloads. This legacy tooling was deprecated in favor of Genny in order to provide:

- A standardized [interface](#org3610c67) for workload authoring in yaml.
- A [phase mechanism](#orgb655d69) for organizing workload execution.
- More reusability via yaml-configurable [Actors](#org51d4d33) with a consistent C++ [authoring mechanism](#org7e6c6bd).
- High-fidelity intrarun [time series outputs](#orgec88ad4).
- Lack of dependency on the legacy mongo shell.

For the above reasons, we encourage users to write new workloads in Genny.

<a id="orgc7904ae"></a>

## What is the system under test?

Genny usually expects to be given a connection string to a system under test. While there's nothing preventing a user from writing a "workload" that spawns the system under test, there is not first-class support for such behavior. Genny is explicitly _not_ a test orchestrator. For test orchestration needs, consider using [DSI](https://github.com/10gen/dsi/).

By default, Genny will try to connect to a MongoDB server at `localhost:27017`. To pass a MongoDB connection string to Genny, use the `-u` option:

    ./run-genny workload -u [arbitrary_URI] src/workloads/docs/InsertWithNop.yml

For more details on how Genny handles connections, see [Connecting to the Server](#orgd6b0450).


<a id="org3610c67"></a>

## What is a workload?

A **workload** is a repeatable procedure that Genny uses to generate load against a system under test. Genny workloads are written in yaml configs that describe how **Actors** move through **phases**. This section describes each of these.


<a id="orgdecc7ae"></a>

### How are workloads configured?

Here is an example of a simple Genny workload config (from the Getting Started section above):

```yaml
SchemaVersion: 2018-07-01
Owner: "@10gen/dev-prod-tips"
Description: |
  This is an introductory workload that shows how to write a workload in Genny.
  This workload writes a few messages to the screen.

Keywords:
- docs
- HelloWorld

Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 1 👬
    Repeat: 100
```

Everything under the `Actor` key (where the magic happens) will be explained in the next section. First let's look at the other **required** keys:

-   `SchemaVersion` - This is a basic versioning system used for Genny workload syntax. For the moment, any new workloads should have the value `2018-07-01` for this key.
-   `Owner` - This should have an identifier for the team that owns the workload, ideally an @-mentionable GitHub team.
-   `Description` - This should contain a written description of the workload. It's recommended to go into as much detail as possible, since understanding the performance issue behind why a workload was written can be difficult months or years later.
-   `Keywords` - These should be searchable keywords associated with your workload. Include keywords for Actors used, operations performed, qualities of the system under test that are expected, etc.

Workload configurations can be found in [./src/workloads](../src/workloads) from the Genny repo root. Organization of this directory is arbitrary as far as Genny is concerned, though example workloads should be in the `docs` subdir.


<a id="org51d4d33"></a>

### What is an Actor?

Genny uses an Actor-based model for its workload generation. When execution begins, Genny spawns all configured Actors in their own threads. Each Actor can behave independently, be configured separately, and even has its own source code. Actors generally have a core loop that performs work against the system under test, and this loop can be repeated many times.

In the example above, the following is the Actor configuration:

```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 1 👬
    Repeat: 100
```

In this example, there is a single `HelloWorld` Actor allocated two threads. This Actor moves through a series of phases, printing a message in each. Phases are described further in the next section. Each thread contains a complete "instance" of the Actor, configured identically. We could add more Actors like so:

```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 1 👬
    Repeat: 100
- Name: InsertRemoveExample
  Type: InsertRemove
  Threads: 100
  Phases:
  - Collection: inserts
    Database: test
    Duration: 10 milliseconds
  - Nop: true
```

This example has an additional `InsertRemove` Actor with 100 threads, where each thread inserts and removes a document as fast as possible. A user observing the database contents might notice a document appearing and disappearing rapidly. This Actor will output `InsertRemoveExample.Insert.ftdc` and `InsertRemoveExample.Remove.ftdc` time series outputs, showing the insertions and removals happening for 10 milliseconds at the phase start. (See [here](#orgec88ad4) for more details on outputs.)

Note that even though the Actors are listed sequentially, all Actors are concurrent. [Example](../src/workloads/HelloWorld-MultiplePhases.yml).

Actor configurations expect the following keys:

-   `Name` - The human-understandable name of this particular Actor configuration. This should be unique throughout the workload.
-   `Type` - The kind of Actor to create. This determines the Actor's behavior and possible configuration options.
-   `Threads` - How many threads to allocate for this Actor.
-   `Phases` - A list of phase configurations (described in next section).

In addition to the universal fields above, individual Actors may have their own configuration keys, such as the `Message` key of the `HelloWorld` Actor, used to determine what message is printed.

Some tips for configuring Actors:

- If your configurations grow complex, consider splitting phases out to a new file. See [here](#org2078b23).
- If you have many threads, make sure you increase the client pool size. If the pool grows large, consider having several. [See here](#orgd6b0450).
- Make sure each Actor has a globally unique name.
- Consider whether your Actor will have "lingering" effects after a phase transition. If it does, and if that's not desireable for this workload, consider using the Quiesce Actor.

Actors are written in C++, and creating new Actors or extending existing ones is a common and encouraged workflow when using Genny. These Actors are owned by their authors. For more details, see [Creating an Actor](#org7e6c6bd).


<a id="orgb655d69"></a>

### What is a phase?

Genny workloads and Actors proceed in a sequence of phases, configured inside Actors. In the running example, our `HelloWorld` Actor is configured with two phases:

```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 1 👬
    Repeat: 100
```

This Actor will execute the first phase for 50 milleseconds. It will perform iterations of its main loop (printing "Hello Phase 0") as many times as it can for that duration. It will then move on to the second phase, where it will perform exactly 200 iterations (100 per thread) of its main loop
(printing "Hello Phase 1"), regardless of how long it takes. Then the workload will end.

Now consider a situation with two Actors:

```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 1 👬
    Repeat: 100
- Name: HelloWorldSecondExample
  Type: HelloWorld
  Threads: 1
  Phases:
  - Message: Other Actor Phase 0
    Duration: 10 milliseconds
  - Message: Other Actor Phase 1
    Duration: 10 milliseconds
```

Here we have the `HelloWorldSecondExample` Actor running for 10 milliseconds in each phase. However, the second phase will not begin after 10 milliseconds. It's important to note that phases are coordinated globally, and Actors configured with either `Repeat` or `Duration` will hold the phase open. In this case, `HelloWorldSecondExample` will operate for 10 milliseconds during the first phase, sleep for 40 milliseconds for the rest of the phase, then after `HelloWorldExample` finishes holding the phase open, both Actors will begin the next phase.

Therefore, the logged output of this Actor would show many lines of `Hello Phase 0` interspersed with `Other Actor Phase 0`, then a long period of _only_ lines showing `Hello Phase 0`, then the next phase would begin with many lines of `Other Actor Phase 1` intersperesed with exactly 200 lines saying `Hello Phase 1`.

Phase configurations accept the following main keys:

-   `Duration` - How long to operate in this phase while holding the phase open.
-   `Repeat` - How many times to repeat the operation while holding the phase open.
-   `Blocking` - This key can be specified with the value `None` to cause the Actor to run as a **background Actor** for this phase. This Actor will act as many times as possible during the phase without holding it open, then move on to the next phase when everyone else is ready.
-   `Nop` - This key can be set with the value `true` to cause the Actor to nop for the duration of the phase.

A couple of notes about the above:

-   You can specify both `Repeat` and `Duration` for a phase. Whichever lasts longer wins.
-   It is undefined behavior if a given phase does not have some Actor specifying `Repeat` or `Duration`.

1.  Sleeping

    In addition to the above keys, Actors can also be configured to sleep during parts of phases. For example:
    
```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 2
  Phases:
  - SleepBefore: 10 milliseconds
    Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
    SleepAfter: 15 milliseconds
```
    
This will sleep for 10 milliseconds at the beginning of *every* Actor iteration and for 15 milliseconds at the end of every iteration. This time is counted as part of the phase duration. Genny accepts the following sleep configurations:
    
-   `SleepBefore` - duration to sleep at the beginning of each iteration
-   `SleepAfter` - duration to sleep after each iteration

2.  Rate Limiting

By default, Actors will repeat their main loop as quickly as possible. Sometimes you want to restrict how quickly an Actor works. This can be done using a rate limiter:
    
```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 100
  Phases:
  - Message: Hello Phase 0
    GlobalRate: 5 per 10 milliseconds
    Duration: 50 milliseconds
```
    
Using the `GlobalRate` configuration, the above Actor will only have 5 threads act every 10 milliseconds, despite having 100 threads that could reasonable act at once. If this workload's outputs were to be analyzed and the intrarun time series were graphed, the user would see only 5 operations occurring every 10 milliseconds. (See [here](#orgec88ad4) for more details about outputs.)
    
In addition to hard-coding how many threads act and when, you can configure Genny to rate-limit the Actor at a percentage of the detected maximum rate:
    
```yaml
Actors:
- Name: HelloWorldExample
  Type: HelloWorld
  Threads: 100
  Phases:
  - Message: Hello Phase 0
    GlobalRate: 80%
    Duration: 2 minutes
```
    
    The above workload will run `HelloWorldExample` at maximum throughput for either 1 minutes or 3 iterations of the Actor's loop, whichever is longer. Afterwards, Genny will use the estimated throughput from that time to limit the Actor to 80% of the max throughput.
    
Note that the rate limiter uses a [token bucket algorithm](https://en.wikipedia.org/wiki/Token_bucket). This means that bursty behavior is possible. For example, if we configure `GlobalRate: 5 per 10 milliseconds` then we will have 5 threads act all at once, followed by 9 or so milliseconds without any threads acting, then another burst of 5 threads acting, etc. We can smooth the rate by specifying a tighter yet equivalent rate limit: `GlobalRate: 1 per 2 milliseconds`.
    
Since the percentage-based limiting treats the entire estimation period as the duration in the rate specification, it is highly prone to bursty behavior.
    
Rate limiting accepts the following configurations:
    
-   `GlobalRate` - specified as either a rate specification (x per y minutes/seconds/milliseconds/etc) or as a percentage


<a id="org32b8ad3"></a>

### How do I run a workload?

Workloads can be run with the following Genny command:

```bash
./run-genny workload [path_to_workload]
```

If your workload requires a MongoDB connection (most do), then you can pass it in with `-u`. See [Connecting to the Server](#orgd6b0450) for more details.


<a id="orgec88ad4"></a>

## Outputs

Genny's primary output is time-series data. Every time an Actor performs an operation, such as an insert, a removal, a runCommand, etc, the Actor thread starts a timer. When the operation returns from the server, the operation is recorded as either a success or failure. The duration of the operation, as viewed from the client, is recorded with the operation completion time.

Genny outputs to `./build/WorkloadOutput`. When running Genny for the first time, you should see two outputs in that directory:

-   `CedarMetrics` - a directory full of FTDC files, where each file corresponds to a single time-series metric for a single operation. For more details about the format and contents of these FTDC files, see our tool-agnostic documentation [here](https://github.com/10gen/performance-tooling-docs/blob/main/getting_started/intrarun_data_generation.md).
-   `workload` - a directory containing the preprocessed workload. Learn more about the preprocessor [here](#org2078b23).

If you run Genny and the `CedarMetrics` directory already exists, it will be moved to `CedarMetrics-<current_time>` to avoid overwriting results. The preprocessed workload will be deposited into the `workload` directory, possibly overwriting the existing one. (Or you may end up with multiple workloads in the directory, if they have different names. This has no impact on execution.)

You can use the `export` command that Genny provides to export outputted FTDC to CSV. For example, to export the results of the Insert operation in the InsertRemove workload as CSV data:

```bash
./run-genny export build/WorkloadOutput/CedarMetrics/InsertRemoveTest.Insert.ftdc -o insert.csv
```

You can also use the `translate` subcommand to convert results to a [t2-readable](https://github.com/10gen/t2/) format.

If you are running Genny through DSI in Evergreen, the FTDC contents are rolled up into summary statistics like `OperationThroughput` and such, viewable in the Evergreen perf UI.

<a id="analyzing-workload-output-locally"></a>

### Analyzing workload output locally

If you are running Genny locally, you can use `src/workloads/contrib/analysis/test_result_summary.py` to print a summary of the most recent run (or any `CedarMetrics` directory) to the console. This script has its own Python dependencies which must be installed as a prerequisite (preferably in a Python virtual environment). The dependencies are captured in [src/workloads/contrib/analysis/requirements.txt](https://github.com/mongodb/genny/blob/master/src/workloads/contrib/analysis/requirements.txt). See [this README](https://github.com/mongodb/genny/blob/master/src/workloads/contrib/analysis/README.md) for more detailed instructions.

After installing the required Python packages, the tool can be run as in the following example:

```
python src/workloads/contrib/analysis/test_result_summary.py -m throughput timers.dur -a ".*Sleep.*" -b 3
SleepTest.SleepTest summary:
        timers.dur (measured in nanoseconds, displayed in milliseconds):
                count     : 160     
                average   : 1001.3  
                median    : 1000.9  
                mode      : 1000.9  
                stddev    : 0.4     
                [min, max]: [1000.4, 1002.5]
                histogram:
                        [1000,1001): ***************************        (27)
                        [1001,1002): ************************************************************...    (128)
                        [1002,1002]: *****      (5)
        throughput:
                ops       : 160.0   
                seconds   : 160.213370114
                ops per second: 0.9987  

```

Try using `python test_result_summary.py --help` for more options.

<a id="org0e7c476"></a>

## Workload Development

1.  Create a yaml file in `./src/workloads` in whatever topical subdirectory you deem appropriate and populate it with appropriate configuration. If you have yaml configuration that may need loading, place it in `./src/phases` (and for more details about what that means, see [here](#org2078b23)). Consider whether existing Actors can be repurposed for your workload, or whether a new one is needed. For the latter, see [here](#org7e6c6bd).

    ```bash
    vim src/workloads/[workload_dir]/[workload_name.yml]
    vim src/phases/[phase_dir]/[phases_name.yml] # Only necessary if creating external configuration
    ./run-genny create-new-actor  # Only necessary if creating a new Actor
    ```

2.  Run the self-tests:

    ```bash
	./run-genny lint-yaml  # Lint all YAML files
	./run-genny cmake-test  # Run C++ Unit test - only necessary if editing core C++ code
	./run-genny resmoke-test  # Run Actor integration tests - only necessary if adding/editing Actors
    ```

    Note the current [issue](#orgb084b49) running resmoke-test. Also, note that there is no schema-checking of the yaml.

3. Update workload documentation.

    ```bash
    ./run-genny generate-docs
    ```

    If changes have been made to the workload name, description, owners, or keywords you need to generate documentation for the workload. Include the generated documentation with your commit.

4.  (Optional) To double-check which Actors run in each Phase, run your workload in dry-run mode with `debug` log level.
This would set up the workload and print it as a list of Phases with the Actors that run in each Phase, then quit:

    ```bash
    ./run-genny workload --dry-run --verbosity debug src/workloads/[workload_dir/workload_name.yml]
    ```

5.  (Optional) If you can run your system under test locally, you can test against it as a sanity-check:

    ```bash
    ./run-genny workload -u [connection_uri] src/workloads/[workload_dir/workload_name.yml]
    ```

6.  (Optional) If you are using DSI, you can run your workload through it by copying or symlinking your Genny directory into your DSI workdir. See [Running DSI Locally](go/running-dsi-locally) for details:

	```bash
	./run-dsi onboarding  # introductory DSI command; see link above for details
	cd WORK
	rm -rf src/genny
	ln -s ~/[path_to_genny]/genny src
	vim bootstrap.yml
	```

7.  Before merging, you should run your workload in realistic situations in CI and check the resultant metrics. For Genny workloads run through DSI using [AutoRun](#org2b04b49), you can create a patch using the following:

	```bash
	cd ~/[path_to_evg_project_repo]
	evergreen patch -p [evg_project]
	cd ~/[path_to_genny]/genny
	evergreen patch-set-module -i [patch_id_number] -m genny
	```

	The `evg_project` above should correspond to the Evergreen project being tested. For example, `sys-perf`.

    You can then select `schedule_patch_auto_tasks` on a variant to schedule any modified or new Genny tasks created by AutoRun. Alternatively, you could select `schedule_variant_auto_tasks` to schedule all Genny tasks on that variant.

    *Note:* [Skipping the compile step in sys-perf projects](https://github.com/10gen/performance-tooling-docs/blob/main/patch_testing.md#skipping-compilation-on-sys-perf-projects) can save some time when testing a genny workload.

For more details on workload development, please check out our general docs on [Developing and Modifying Workloads](https://github.com/10gen/performance-tooling-docs/blob/main/new_workloads.md) and on [Basic Performance Patch Testing](https://github.com/10gen/performance-tooling-docs/blob/main/patch_testing.md).

Users who would like a second look at their workloads can ask product performance. Users who have questions about their Genny usage can ask DEVPROD in #ask-devprod-performance.

Users who would like a private workload should consider putting it in the [PrivateWorkloads repo](https://github.com/10gen/PrivateWorkloads).


<a id="org61c719c"></a>

# Further Concepts


<a id="org78b250a"></a>

## Common Actors

There are several Actors owned by DEVPROD which are intended for widespread use:

-   CrudActor - Used to perform CRUD operations, recording client-side metrics.
-   RunCommand - Execute a command against the remote server. Often used for utility purposes, but metrics are collected as well.
-   Loader - Load many documents into the remote database. Often used early in a workload to set the preconditions for testing.
-   QuiesceActor - Quiesce a cluster, making sure common operations are complete. This is often used to reduce noise between phases.

Examples with these and other Actors can be found in [./src/workloads/docs](../src/workloads/docs).


<a id="org2b04b49"></a>

## AutoRun


<a id="orgd0067d1"></a>

### What is AutoRun?

AutoRun is a utility to allow workload authors to determine scheduling of their workloads without having to commit to a separate repo. The utility is specifically designed for users who are using DSI through Evergreen. To use AutoRun, make sure your project has integrated DSI with Evergreen as explained [here](https://github.com/10gen/dsi/wiki/DSI-In-Evergreen).

AutoRun searches in `[workspace]/src/*/src/workloads` and assumes that all workload repos, including Genny itself, are checked out in `[workspace]/src`.

After performing the above integration, your Evergreen project should have a `schedule_variant_auto_tasks` task on each variant, which can be used to schedule all Genny workloads that are configured to run on this variant. There will also be the `schedule_patch_auto_tasks` task which will schedule any new or modified Genny workloads. If you want to run an unmodified workload, make a small edit (such as inserting whitespace) to force it to be picked up by that latter task.

Both of the above tasks will have a dependency on `schedule_global_auto_tasks`, which invokes `./run-genny auto-tasks` to create all possible tasks, viewable in the `TaskJson` directory of that task's DSI artifacts. The variant-specific task generator task will then schedule the appropriate task, based on the workload configurations described below. Note that task execution only occurs once per patch, even if a task is re-executed. If testing changes to task generation configuration, creating a new patch will be necessary.



<a id="orgbfb0d8e"></a>

### Configuring AutoRun

The `schedule_variant_auto_tasks` task automatically runs workloads based on the evergreen environment
(variables from `bootstrap.yml` and `runtime.yml` in DSI) and an optional AutoRun
section in any workload. The AutoRun section is a list of `When`/`ThenRun` blocks,
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
      - v6.1
      $gte: v5.0
  ThenRun:
  - infrastructure_provisioning: foo
  - infrastructure_provisioning: bar
  - arbitrary_key: baz
```

In this case, it looks in the `bootstrap.yml` of `test_workload`, checks if `mongodb_setup`
is either `replica` or `replica-noflowcontrol`, and also if `branch_name` is not `v6.1` and also is
greater or equal to `v5.0`.
If all conditions are true, then we schedule several tasks. Let's say the workload name is
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

-   Supports multiple `When`/`ThenRun` blocks per `AutoRun`. Each are evaluated independently.
-   `When` blocks can evaluate multiple conditions. All conditions must be true to schedule the task.
-   `When` supports `$eq`, `$neq`, `$gte`, `$gt`, `$lte` and `$lt`.
-   `$eq` and `$neq` can accept either a scalar or list of values.
-   `$gte`, `$gt`, `$lte` and `$lt` can accept only scalar values.
-   For a list of values, `$eq` evaluates to true if it is equal to at least one.
-   For a list of values, `$neq` evaluates to true if it is equal to none of the values.
-   `$gte`, `$gt`, `$lte` and `$lt` use either regular comparison or version comparison.
-   Version comparison is used when both arguments are version strings. Version strings have format
    `vA.B` where `A` and `B` are integers. When comparing two version strings, first `A` values are
    compared and only if they are equal, `B` values are compared.
-   Special strings `master` and `main` are also considered version strings which are greater
    than all other version strings.
-   `ThenRun` blocks are optional.
    -   ****Most usecases do not need to use ThenRun****
    -   If you do use `ThenRun`, please be judicious. If you have a task that is scheduled when
        `mongodb_setup` == `replica`, it would be confusing if `mongodb_setup` was overwritten to `standalone`.
        But it would be ok to overwrite `mongodb_setup` to `replica-delay-mixed`.
-   Each item in the `ThenRun` list can only support one `{bootstrap_key: bootstrap_value}` pair.
-   If using `ThenRun` but you would also like to schedule a task without any bootstrap overrides,
    Add an extra pair to `ThenRun` with the original key/value.
-   If using `ThenRun`, the new task name becomes `<taskname>_<bootstrap-value>`. In the `ParallelWorkload` example,
    the task name becomes `parallel_insert_replica_delay_mixed` (name is automatically converted to snake_case).
    The `bootstrap-key` is not included in the name for the purpose of not changing existing names and
    thus deleting history. This may change after PM-2310.


<a id="orgd89f221"></a>

## Value Generators

It is often necessary to use Genny to operate with large amounts of data which would be impractical to hardcode. Genny uses value generators for this. A value generator is a piece of code that generates pseudorandom values every time it is invoked, and which can be configured from the workload yaml. Notably, value generators use a hardcoded seed, which is always automatically reused by default, so repeated Genny executions should be deterministic with respect to generated values. A user wanting to vary the generated docs can vary the seed. See [./src/workloads/docs/GeneratorsSeeded.yml](../src/workloads/docs/GeneratorsSeeded.yml) for an example.

Value generators are not a builtin feature of Genny, but must be integrated by each Actor for the configuration values that accept them. For examples of using value generators, see [./src/workloads/docs/Generators.yml](../src/workloads/docs/Generators.yml). To integrate generators into an Actor, use the [DocumentGenerator](../src/value_generators/include/value_generators/DocumentGenerator.hpp) with the yaml node you intend to generate documents from. (And see [here](#org7e6c6bd) for more details on creating an Actor in the first place.)

You can also find some example datasets that can be used with generators like `ChooseFromDataset`. For more information check [./src/workloads/datasets/README.md](../src/workloads/datasets/README.md).

<a id="org2078b23"></a>

## Preprocessor

For convenience when developing workloads, Genny offers a preprocessing syntax that can be used for configuration reuse and parameterization. Remember: when developing a workload, you can always check results of preprocessing:

```bash
./run-genny evaluate src/workloads/[workload_dir/workload_name.yml]
```
	
This command helps find yaml-based mistakes. Note that we don't do schema-checking for this yaml.


<a id="orga6d35c7"></a>

### LoadConfig

The `LoadConfig` keyword can be used to load arbitrary configuration from another file. For example, consider the following Actor definition:

```yaml
Actors:
- Name: HelloWorld
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - LoadConfig:
      Path: ../../phases/HelloWorld/ExamplePhase2.yml
      Key: UseMe  # Only load the YAML structure from this top-level key.
      Parameters:
        Repeat: 2
```

Also consider the following file located at `./src/phases/HelloWorld/ExamplePhase2.yml`:

```yaml
SchemaVersion: 2018-07-01
Description: |
  Example phase to illustrate how PhaseConfig composition works.

UseMe:
  Message: Hello Phase 2
  Repeat: {^Parameter: {Name: "Repeat", Default: 1}}
```

Using `LoadConfig`, the contents of the `UseMe` key will be placed into the location where the `LoadConfig` was evaluated, with parameters substituted, so we end up with the following ouput from evaluation: 

```yaml
Actors:
- Name: HelloWorld
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 50 milliseconds
  - Message: Hello Phase 2
    Repeat: 2
```

A few notes:

-   The parameter `Repeat` was substituted in. A loaded config can have any number of parameters substituted, at any key's value.
-   The loaded config filepath should be relative to the location of the workload containing `LoadConfig`.
-   The contents of the loaded config are shallow-merged into the location where the `LoadConfig` is evaluated. If there are conflicting keys at the location, the existing key-values are kept. There is no deep dict merge at present.

The `LoadConfig` keyword can be used to substitute and parameterize anything, including entire workloads! For an example of this, see [here](../src/workloads/docs/HelloWorld-LoadConfig.yml).


<a id="orga45b1d8"></a>

### ActorTemplate

Genny also offers a syntax for templatizing Actors. This is useful if there are many Actors that share common configuration, which need to differ in specific ways. An example of this can be found [here](../src/workloads/docs/HelloWorld-ActorTemplate.yml).


<a id="orgf9c328f"></a>

### OnlyActiveInPhases

If there are many phases, and an Actor only needs to run for some of them, there is an alternative syntax to specify only the phases the Actor runs in. Consider the following Actor:

```yaml
Actors:
- Name: HelloWorld
  Type: HelloWorld
  Threads: 2
  Phases:
    OnlyActiveInPhases:
      Active: [0, 2]
      NopInPhasesUpTo: 3
      PhaseConfig:
        Message: Alternate Phase 1
        Repeat: 100
```

This configures the Actor to run with the given configuration in phases named 0 and 2, and nops in all other phases up phase named 3.

<a id="preprocessorflattenonce"></a>

### FlattenOnce

You can also flatten arrays to reduce their dimensionality by 1 with `FlattenOnce`. For example:

```yaml
Actors:
  FlattenOnce:
  - - ActorFromTemplate: {TemplateName: Setup1}
    - ActorFromTemplate: {TemplateName: Setup2}
    - ActorFromTemplate: {TemplateName: Setup3}
  - - ActorFromTemplate: {TemplateName: Workload1}
    - ActorFromTemplate: {TemplateName: Workload2}
  - ActorFromTemplate: {TemplateName: Workload3}
  - Name: Quiesce
    Type: QuiesceActor
    Threads: 1
    Database: *db
    Phases:
      OnlyActiveInPhases: [7]
      NopInPhasesUpTo: 6
      PhaseConfig:
        Repeat: 1
```

will flatten down to

```yaml
Actors:
- ActorFromTemplate: {TemplateName: Setup1}
- ActorFromTemplate: {TemplateName: Setup2}
- ActorFromTemplate: {TemplateName: Setup3}
- ActorFromTemplate: {TemplateName: Workload1}
- ActorFromTemplate: {TemplateName: Workload2}
- ActorFromTemplate: {TemplateName: Workload3}
- Name: Quiesce
  Type: QuiesceActor
  Threads: 1
  Database: *db
  Phases:
    OnlyActiveInPhases: [7]
    NopInPhasesUpTo: 6
    PhaseConfig:
      Repeat: 1
```

This only flattens the outermost array, so inner arrays are left unflattened. You can also use this to hack together a pseudo-repeat. For example:

```yaml
GlobalDefaults:
  One10x: &One10x [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]
  One100x: &One100x [One10x, One10x, One10x, One10x, One10x, One10x, One10x, One10x, One10x, One10x]
  One1000x: &One1000x [One100x, One100x, One100x, One100x, One100x, One100x, One100x, One100x, One100x]
  One10000x: &One10000x [One1000x, One1000x, One1000x, One1000x, One1000x, One1000x, One1000x, One1000x, One1000x, One1000x]
```

<a id="preprocessorformatstring"></a>

### PreprocessorFormatString

`PreprocessorFormatString` behaves similarly to the `FormatString` generator, but at the
preprocessor level. This is helpful particularly when you need to autogenerate sensible names for
actors or something, but want to eliminate as much redundancy as possible. For example:

```yaml
ActorTemplates:
- TemplateName: MultithreadedWorkload
  Config:
    Name: {^PreprocessorFormatString: {format: "MTWorkload%04dT", withArgs: {^Parameter: {Name: Threads, Default: 1}}}}
    Type: CrudActor
    Threads: {^Parameter: {Name: Threads, Default: 1}}
    Phases: {^Parameter: {Name: Phases}}

Actors:
- ActorFromTemplate:
    TemplateName: MultithreadedWorkload
    TemplateParameters:
      Threads: 16
      Phases: {...}
- ActorFromTemplate:
    TemplateName: MultithreadedWorkload
    TemplateParameters:
      Threads: 512
      Phases: {...}
- ActorFromTemplate:
    TemplateName: MultithreadedWorkload
    TemplateParameters:
      Threads: 4096
      Phases: {...}
```

will generate three actors, with names MTWorkload0016T, MTWorkload0512T, and MTWorkload4096T.

<a id="preprocessornumexpr"></a>

### NumExpr

The preprocessor can also precompute simple numerical expressions for you.

```yaml
GlobalDefaults:
  MaxThreads: &MaxThreads 1024

ActorTemplates:
- TemplateName: ReadWriteWorkload
  Config:
    Type: CustomReadWriteActor
    Threads: *MaxThreads
    Phases:
      OnlyActiveInPhases:
        Active: [{^Parameter: {Name: Phase, Default: {unused: ...}}}]
        NopInPhasesUpTo: 10
        PhaseConfig:
          Readers: {^Parameter: {Name: Readers, Default: *MaxThreads}}
          Writes: {^NumExpr: {withExpression: "total - readers", andValues: {total: *MaxThreads, readers: {^Parameter: {Name: Readers, Default: *MaxThreads}}}}}
```

This can help ensure numerical invariants, such as `Readers + Writers = Threads` in the actor
template above.

<a id="org22b7a0f"></a>

### Defaults and Overrides

By default, a Genny workload yaml contains the following configuration:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 100
```

For more details about this configuration's purpose, see the section on [Connecting to the Server](#orgd6b0450).

Genny also has an override syntax for configuring workloads. When invoking Genny, you can use the `-o` option to specify an override file.
This uses [OmegaConf](https://omegaconf.readthedocs.io/en/2.1_branch/) to merge the override file onto the workload. This functionality
should only be used to set values that absolutely need to be specified at runtime, such as URIs for systems under test. (See [Connecting to the Server](#orgd6b0450) for details.)

*Warning* - Using overrides and omegaconf syntax and greatly increase workload complexity. Users interested in using overrides files or OmegaConf functionality should come talk to the DEVPROD team.

Furthermore, there is a default Actor that is injected during preprocessing, which has the following configuration:

```yaml
Name: PhaseTimingRecorder 
Type: PhaseTimingRecorder
Threads: 1
```

This Actor is used to collect several internal metrics.

When actually evaluating and constructing a workload at runtime, Genny takes the following steps:

1.  Start with the defaults.
2.  Apply the workload yaml configuration over the defaults, deep merging the yamls and giving priority to the workload yaml.
3.  Apply the overrides file (if given) over the results of step 2, deep merging the yamls and giving priority to the overrides.
4.  Use the preprocessor on the resultant config, evaluating all `LoadConfig`, `ActorTemplate`, and other keywords recursively. Injection of the `PhaseTimingRecorder` default Actor occurs while evaluating the `Actors` list.
5.  Output the result to `./build/WorkloadOutput/workload`.
6.  Run the workload.

Note that the final results of the above can be viewed with `./run-genny evaluate`. This is extremely useful for debugging.


<a id="orgd6b0450"></a>

## Connecting to the Server


<a id="orgd2659db"></a>

### Connection Strings and Pools

Genny creates connections using one or more C++ driver pools. These pools can be configured in a workload like so:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
  Update:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
```

This will configure two pools, one named `Default` and one named `Update`. The `QueryOptions` can contain
any supported [connection string option](https://docs.mongodb.com/manual/reference/connection-string/), and these will
be spliced into the final URI used to connect. Genny constructs pools lazily, so these pools will not actually be
created until a workload Actor requests them.

The `Default` pool is what Actors connect to unless their .cpp class determines otherwise. Some Actors (the Loader, CrudActor, and RunCommand actors)
have a `ClientName` field for specifying which pool to request.

Genny's preprocessor operates on this configuration to make using it easier. The `-u` CLI option can be used to set the default URI. During preprocessing,
any pool that does not have the `URI` key set will be given this value. The default value of the default URI is `"mongodb://localhost:27017"`. Since URI
is typically not known until runtime, this means that most workloads should have a configuration more like the following:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
  Update:
    QueryOptions:
      maxPoolSize: 500
```


<a id="orga591018"></a>

### Multiple Connection Strings

If multiple different connection strings are needed, such as when testing a multitenant system, we can use an override file. For the above configuration,
we can create the following override:

```yaml
Clients:
  Default:
    URI: "mongodb://localhost:27017"
  Update:
    URI: "mongodb://localhost:27018"
```

Notice the different ports. This can be used at runtime as:

```bash
./run-genny workload example.yml -o override.yml
```

This will apply the override onto the workload, creating the following result:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
  Update:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27018"
```

Genny's `evaluate` subcommand can always be used to see the result of complex configurations.


<a id="org65830c2"></a>

### Default

Since Actors generally need a connection and not all workloads need a complicated connection or multiple pools,
simply not setting any connection pools will cause Genny to default to the following:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 100
    URI: "mongodb://localhost:27017"
```

For more information, see [Defaults and Overrides](#org22b7a0f).


<a id="org7e6c6bd"></a>

## Creating an Actor

Creating new Actors is a common and encouraged workflow in Genny. To create one, run the following:

```bash
./run-genny create-new-actor
```

This will create new Actor .cpp and .h files, an example workload yaml, as well as Actor integration tests, all with inline comments guiding you through the Actor creation process. You might want to take a look at [Developing Genny](./developing.md) and the [Contribution Guidelines](../CONTRIBUTING.md).

If your configuration wants to use logic, ifs, or anything beyond simple or existing commands in a loop, then consider writing your own Actor. It doesn't need to be super general or even super well-tested or refactored. Genny is open to submissions and you own whatever Actor you write. No need to loop DEVPROD in to your custom actor's PR unless you'd just like a second look.


<a id="org627591d"></a>

## Enabling Client-Side Field Level Encryption

To enable encryption in your workload using either Field Level Encryption (FLE) or Queryable Encryption (QE), the schema for the encrypted collection must first be defined in the `Encryption.EncryptedCollections` node of the workload YAML file. The following example shows a FLE schema defined for namespace `encrypted_db.fle_encrypted_coll`, and a QE schema defined for namespace `encrypted_db.qe_encrypted_coll`:

```yaml
Encryption:
  EncryptedCollections:
  - Database: encrypted_db
    Collection: fle_encrypted_coll
    EncryptionType: fle
    FLEEncryptedFields:
      name: { type: "string", algorithm: "random" }
      pii.ssn: { type: "string", algorithm: "deterministic" }
  - Database: encrypted_db
    Collection: qe_encrypted_coll
    EncryptionType: qe
    QueryableEncryptedFields:
      name: { type: "string", queries: [{queryType: "equality"}] }
      pii.ssn: { type: "string", queries: [{queryType: "equality"}] }
```

The required `Database` and `Collection` fields specify the namespace of the encrypted collection. Each node in this sequence must have a unique combination of these two names in order to avoid two different encryption schemas for the same namespace on the same URI.

The `FLEEncryptedFields` and `QueryableEncryptedFields` fields hold the schema definitions for FLE and QE, respectively. Both can be specified, however only one of them is applied to the collection during the workload. This is determined by the required `EncryptionType` field, which can be set to `"fle"` or `"qe"` to apply the schema in either the `FLEEncryptedFields` or `QueryableEncryptedFields`, respectively.

Once the schema is defined for an encrypted collection, encryption must be enabled in every connection pool defined under `Clients` that will be running operations on this collection. To do this, provide an `EncryptionOptions` node under the client definition as follows:

```yaml
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 100
    EncryptionOptions:
      KeyVaultDatabase: "keyvault"
      KeyVaultCollection: "datakeys"
      EncryptedCollections:
      - encrypted_db.fle_encrypted_coll
      - encrypted_db.qe_encrypted_coll
```
In the above example, we enable encryption in the `Default` client pool. The `EncryptionOptions` node requires that the namespace of the key vault be specified via the `KeyVaultDatabase` and `KeyVaultCollection` fields. It also requires that the encrypted namespaces that will be operated on through this client be listed under the `EncryptedCollections` field. These namespaces must have a corresponding definition in `Encryption.EncryptedCollections`.

During client pool setup, key vaults in each unique URI are dropped & created once, when the first client pool for that URI is created. Data keys for encrypted namespaces are generated only once, if the associated key vault & URI does not yet contain keys for that namespace. This means that if two client pools have a similar URI and key vault namespace, then the encrypted collections they have in common will be using the same data keys.

An Actor that wishes to perform encrypted operations on an encrypted collection must be using a client pool with an `EncryptedCollections` subnode that includes the collection's
namespace.

By default, encrypted CRUD operations require a `mongocryptd` daemon to be running and listening on localhost:27020. Genny will **not** automatically spawn this daemon on a workload run. Alternatively, one can set `Encryption.UseCryptSharedLib` to `true`, and provide the path to the `mongo_crypt_v1.so` shared library file using `Encryption.CryptSharedLibPath`.

For a full example of an encrypted workload, see [here](../src/workloads/docs/CrudActorEncrypted.yml).

<a id="org3aaae9e"></a>

# Pitfalls


<a id="orga7ab911"></a>

## pipe creation failed (24): Too many open files

If you see errors like this locally, try either increasing your ulimit or reducing the number of threads and duration.


<a id="orgb084b49"></a>

## Actor integration tests fail locally

There are currently pathing issues when running integration tests locally. This is tracked in [TIG-3687](https://jira.mongodb.org/browse/TIG-3687). That ticket also lists a workaround for local use.


<a id="org97681a9"></a>

## The Loader agent requires thread count set on both Actor and phase level

This is tracked in [TIG-3016](https://jira.mongodb.org/browse/TIG-3016) which will correct the issue.

<a id="org97681a9"></a>

## Two similarly-named workloads are not permitted to coexist

This is tracked in [TIG-3700](https://jira.mongodb.org/browse/TIG-3700) which will correct the issue. 
Note that the failure symptom when this occurs could be an infractrue provisioning error, even though the issue is unrelated to provisioning. 
