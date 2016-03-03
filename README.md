Workload Generation
===================

Overview
--------

This tool is a workload generator for testing the performance of a
mongo cluster. The tool allows you to specify a workload in a yaml
file, with a large degree of flexibility. The workload tool is written
in C++ and wraps the
[C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master),
which in turn supports the
[mongo driver spec](https://github.com/mongodb/specifications/blob/master/source/crud/crud.rst). It
is the intention that:

* Workloads are completely described in YAML (no C or other code
  needed).
* It is easy and fast to define new workloads or change an existing workload.
* Workloads can be composed and defined hierarchically.
* Workloads run consistently fast (the underlying engine is C++), so they test the
  server, not the workload tool.
* The tool scales to large thread counts up to hardware constraints.
* The tool generates reproducible workload executions (seeded pseudo-random number
  generator per thread).
  * Note: The workload generation tool cannot control if the server
    under test responds differently because of factors not under the tools
    control.

There are three basic components in a workload, enabling the
specification of arbitrarily complex workloads in a graph of
operations.

1. [Nodes](Nodes.md): These are nodes in the workload graph. They may
   do something, such as a find_one, or they may control the flow of
   execution, such as random_choice.
2. [Workloads](Workloads.md): These are collection of nodes and represent an actual
   workload. They specify the node that should execute first in the
   workload. They also supply parameters and a pseudo-random number
   generator to the nodes and ops.
3. [Documents](Documents.md): These are objects that can be used anywhere a bson
   Object is needed.

There are a number of [examples](examples/README.md) that demonstrate
the basic ideas. All the examples work. Additional, there is a [tutorial](Tutorial.md)

### Stats

The duration of every node execution is measured and recorded. At the
end of the run, the following stats are reported per node (and nested
workload):

1. Count of executions
2. Average execution time in microseconds
3. Min and Max exection time in microseconds
4. Std. Deviation of execution time in microseconds

Those results are reported to a configurable results json file
(default results.json), and to the logging output at the info
level. They can also be printed out periodically using the -p option.

#### Future stats

The following stats and stat features are not implemented yet.

1. Histogram data
2. Percentile data (95th, 99th percentile), based on histogram data.

I also intend to do the following:

1. Rewrite the stats to be accumulating, rather than resetting. This
   will allow for easy analysis of different regions of behavior.
2. Add the ability for a phase marker node to trigger a stats
   dump. This will enable specifically measuring stats for different
   phases of execution.

The two points combined will be enable us to   gather periodic stats,
phase based stats, and overall stats at the same time. It will require
post-processing to generate useful statistics. A script will be
included to perform the post-processing.

### Future features

This is an incomplete list.

2. More types of transformation of values and tools for building
   documents (hashing, concatenate, adding fields to an existing document)
3. Use of variable in any field (currently only in overrideDocument).
4. Access state from an embedded workload (e.g., give 5 copies of an
   embedded workload unique sequence ids by incrementing a field)
5. See [Operations](Operations.md), [Nodes](Nodes.md),
   [Workloads](Workloads.md), and [Documents](Documents.md) for
   additional future features.

Dependencies
------------

* The
  [C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master). Currently
  tested against r3.0.0. If compiling on OS X, see
  [CXX-836](https://jira.mongodb.org/browse/CXX-836). The C++11 driver
  in turn requires the c driver.
* [yaml-cpp](https://github.com/jbeder/yaml-cpp)
* A compiler that supports c++11
* [cmake](http://www.cmake.org/) for building
* [Boost](http://www.boost.org/) for logging and filesystem
  libraries. Version 1.48 or later

Install Script
--------------

Jonathan Abrahams has added an [install script](install.sh) that
should install all the dependencies and build the tool for CentOS,
Ubuntu, Darwin, and Windows_NT.

Building
--------

    cd build
    cmake ..
    make
    make test # optional. Expects a mongod running on port 27017


Build Notes:

* The build will use static boost by default, and static mongo c++
  driver libraries if they exist. If boost static libraries don't
  exist on your system, add "-DBoost\_NON\_STATIC=true" to the end of your
  cmake line.
* On some systems I've had trouble linking against the mongo c
  driver. In those cases, editing libmongo-c-1.0.pc (usually in
  /usr/local/lib/pkgconfig/) can fix this. On the Libs: line, move the
  non mongoc libraries after "-lmongoc-1.0".

Running
-------

    Usage: ./mwg [-hldrpv] /path/to/workload [workload to run]
    Execution Options:
    	--collection COLL      Use Collection name COLL by default
    	--database DB          Use Database name DB by default
    	--dotfile|-d FILE      Generate dotfile to FILE from workload and exit.
    	                       WARNING: names with spaces or other special characters
    	                       will break the dot file
    	--help|-h              Display this help and exit
    	--host Host            Host/Connection string for mongo server to test--must be a
    	                       full URI,
    	--loglevel|-l LEVEL    Set the logging level. Valid options are trace,
    	                       debug, info, warning, error, and fatal.
    	--numThreads NUM       Run the workload with NUM threads instead of number
    	                       specified in yaml file
    	--resultfile|-r FILE   FILE to store results to. defaults to results.json
    	--resultsperiod|-p SEC Record results every SEC seconds
    	--runLengthMs NUM      Run the workload for up to NUM milliseconds instead of length
    	                       specified in yaml file
    	--variable VAR=VALUE   Override the value of yaml node VAR with VALUE. May be called
    	                       multiple times. If you override a node that defines a YAML
    	                       anchor, all aliases to that anchor will get the new value
    	--version|-v           Return version information

In particular, please note that the command line enables:

* Running multiple workloads from the same file. By default the tool
  will run the workload at node _main_, but you can explicitly select
  the workload to run by specifying it as a second argument to
  mwg. You specify the name of the YAML node that defines the
  workload. This is particularly useful when building and testing
  hierarchically defined workloads, as it enables testing an embedded
  workload by itself.
* Overriding specific settings in the yaml. The collection name, database name,
  runLengthMs, and numThreads for the top level workload can be
  explicitly set from the command line.
* Overriding nodes in the yaml file. The _--variable_ option enables
  changing the value of a node inside the yaml file. The effect is
  equivalent to having edited that line. If the overriden node defined
  an anchor, all aliases to that anchor are also updated. This is a
  very flexible way to change things in your workload.
* Enable periodic stats reporting and saving. Setting the
  _--resultsperiod_ flag enables this. When enabled the results.json
  is a json array, with one entry for every period in the run.

Including YAML Files
--------------------

It is possible to include existing yaml files from another yaml
file. This is done by having a top level YAML node named
_includes_. The entry should be a YAML sequence, and each item in the
sequence should have a _filename_ entry, and either a _node_ with the
name of a node, or _nodes_ with a sequence of node names. The program
will load the YAML from _filename_, and replace the value of _node_ or
the values in _nodes_ with the nodes matching those names in the other
file. To use an included node, assign it a YAML anchor, and use an
alias to that anchor in the desired location. It should look like
this:

    includes:
      - filename: sample1.yml
        node: &sample1 main
      - filename: sample2.yml
        nodes:
          - &delay delay
          - &sample2 main

In this exacmple the node _main_ from the file _sample1.yml_ will be
available in includes[0]["node"]. The anchor _sample1_ is defined, an
the main node from sample1.yml may now be referenced with the alias
"*sample1". See [includes2.yml](examples/includes2.yml) and
[includes.yml](examples/includes.yml) for complete examples in the
example directory.

Examples
--------

There are a collections of examples in the [examples directory](examples/). To run
the [forN.yml](examples/forN.yml) example, simply do:
    ./build/mwg examples/forN.yml

The workloads are all specified in yaml. An example find node is:

    name: find
    type: find
    filter: { x : a }
    next: sleep

The main parts of this are:

* name: This is a label used to refer to the node or operation. If not
  specified, a default name will be provided.
* type: This says what the operation should be. Basic operations
  include the operations supported by the C++11 driver. Currently a
  subset is supported. See [Operations.md](Operations.md) for more.
* filter: This is specific to the find operation. The argument will be
  converted into bson, and will be used as the find document passed
  to the C++11 driver when generating the find
* next: This is the node to transition to when completing this
  operation. If not specified, the immediately following node in the
  definition will be set as the next node.

A simple workload that does an insert and a find, and randomly
chooses between them:

    seed: 13141516
    name: simple_workload
    nodes:
         - name: insert_one
           type: insert_one
           document: {x: a}
           next: choice
         - name: find
           type: find
           find: {x: a}
           next: choice
         - name: choice
           type: random_choice
           next:
               find: 0.5
               insert_one: 0.5

This workload will run forever, and after each operation make a random
choice of whether to do an insert_one or find next. The workload can be
made finite by adding an absorbing state.

    seed: 13141516
    name: simple_workload
    nodes:
         - name: insert_one
           type: insert_one
           document: {x: a}
           next: choice
         - name: find
           type: find
           find: {x: a}
           next: choice
         - name: choice
           type: random_choice
           next:
               find: 0.45
               insert_one: 0.45
               Finish: 0.1

The Finish state is an implicit absorbing state. The workload will
stop when it reaches the Finish state.
