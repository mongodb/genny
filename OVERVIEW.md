Overview
========

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


Stats
=====

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

