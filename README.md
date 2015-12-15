Workload Generation
===================

Overview
--------

This tool is a workload generator for testing the performance of
a mongo cluster. The tool allows you to specify a workload in a yaml file, with
a large degree of flexibility. The workload tool is written in C++ and
wraps the
[C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master),
which supports the
[mongo driver spec](https://github.com/mongodb/specifications/blob/master/source/crud/crud.rst).

There are five basic components in a workload, enabling the
specification of arbitrarily complex workloads in a graph of
operations.

1. [Operations](Operations.md): These are basic operations, such as a find_one. These
   operations *do* something
2. [Nodes](Nodes.md): These are nodes in the workload graph. They often have an
   associated operation. Nodes control the order of execution of the
   operations.
3. [Workloads](Workloads.md): These are collection of nodes and represent an actual
   workload. They specify the node that should execute first in the
   workload. They also supply parameters and a pseudo-random number
   generator to the nodes and ops. 
4. [Documents](Documents.md): These are objects that can be used anywhere a bson
   Object is needed.
5. Variables: A workload has a list of variables that be used and
   set. Variables can be thread local, or shared across the
   workload. Variables are described in [Workloads](Workloads.md)

There are a number of [examples](examples/README.md) that demonstrate
the basic ideas. All the examples work. 

### Stats

Currently stats are limited to logging node and workload durations
during test execution. 

The next step is to collect those durations and collect the simple
accumulated stats per node and workload:
1. Numer of operations
2. Min, max, average, and variance of latency.

Longer term we should add more advanced statistics, histograms, and an
ability to collect stats by time period (e.g., once a minute).

### Future features
This is an incomplete list. 

1. Stats: There are currently no client stats collected beyond the
   timestamps in the logging output
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
* The [C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master)
* [yaml-cpp](https://github.com/jbeder/yaml-cpp)
* A compiler that supports c++11
* [cmake](http://www.cmake.org/) for building
* [Boost](http://www.boost.org/) for logging. Version 1.48 or later

Building
--------

    cd build
    cmake ..
    make

Examples
--------

There are a collections of examples in the [examples directory](examples/). To run
the [forN.yml](examples/forN.yml) example, simply do:
    ./build/mwg examples/forN.yml

The workloads are all specified in yaml. An example find node is:

        name : find
        type : find
        filter : { x : a }
        next : sleep

The main parts of this are:
* name: This is a label used to refer to the node or operation. If not
  specified, a default name will be provided.
* type: This says what the operation should be. Basic operations
  include the operations supported by the C++11 driver. Currently only
  a limited subset is supported.
* filter: This is specific to the find operation. The argument will be
  converted into bson, and will be used as the find document passed
  to the C++11 driver when generating the find
* next: This is the node to transition to when completing this
  operation. If not specified, the immediately following node in the
  definition will be set as the next node.

A simple workload that does an insert and a find, and randomly
chooses between them:

    seed : 13141516
    name : simple_workload
    nodes :
         - name : insert_one
           type : insert_one
           document : {x : a}
           next : choice
         - name : find
           type : find
           find : {x : a}
           next : choice
         - name : choice
           type : random_choice
           next :
               find : 0.5
               insert_one : 0.5

This workload will run forever, and after each operation make a random
choice of whether to do an insert_one or find next. The workload can be
made finite by adding an absorbing state.

    seed : 13141516
    name : simple_workload
    nodes :
         - name : insert_one
           type : insert_one
           document : {x : a}
           next : choice
         - name : find
           type : find
           find : {x : a}
           next : choice
         - name : choice
           type : random_choice
           next :
               find : 0.45
               insert_one : 0.45
               Finish : 0.1

The Finish state is an implicit absorbing state. The workload will
stop when it reaches the Finish state. 
