Workload Generation
===================

Overview
--------

This tool is a workload generator for testing the performance of
mongod. The tool allows you to specify a workload in a yaml file, with
a large degree of fleibility. The workload tool is written in C++ and
wraps the
[C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master),
which supports the
[mongo driver spec](https://github.com/mongodb/specifications/blob/master/source/crud/crud.rst).

There are five basic components in a workload, enabling the
specification of arbitrarily complex workloads in a graph of
operations. 
1. Operations: These are basic operations, such as a find_one. These
   operations *do* something
2. Nodes: These are nodes in the workload graph. They often have an
   associated operation. Nodes control the order of execution of the
   operations.
3. Workloads: These are collection of nodes and represent an actual
   workload. They specify the node that should execute first in the
   workload. They also supply parameters and a pseudo-random number
   generator to the nodes and ops. 
4. Documents: These are objects that can be used anywhere a bson
   Object is needed.
5. Variables: A workload has a list of variables that be used and
   set. Variables can be thread local, or shared across the workload. 

Operations can be embedded in a node for compactness of
representation. The tool will detect this and create an opNode,
containing the operation. 

### Currently supported operations

1. find
2. insert_one
3. insert_many
3. sleep: sleep for some number of milliseconds. The entry sleep
   specifies how long to sleep. 

### Currently supported nodes

1. opNode: This is the default node. It executes its associated
   operation, and then moves execution to its next node.
2. forN: This node wraps a workload. It executes the workload N times
3. randomChoice: This node has a list of other nodes and
   probabilities. It uses the pseudo-random number generator to select
   the next node to execute.
4. Finish: This represents an absorbing state in the graph. The
   workload is finished when the Finish node executes. There is always
   an implicit Finish state included in the workload. 

### Currently supported Documents

1. bsonDocument: This is a default document specified using json and
   converted into bson.
2. overrideDocument: This is a document that wraps another document,
   and can change arbitrary fields in a subdocument. Can currently
   update nested fields except for fields nested in an array. There
   are currently two kinds of overrides:
   1. randomint: Generate a random integer. Accepts min and max
      values.
   2. increment: Takes a variable and uses it's value and does a post
     increment.

### Currently supported workload features

1. numThreads: Specify the number of parallel threads of the workload
   to run.
2. Set the random seed
3. Specify integer variables:
   1. Thread specific variables
   2. Workload wide variables

### Currently unsupported features
This is an incomplete list

1. Setting the time limit to run the workload
2. Stats. There are currently no client stats collected
3. Generating or modifying documents. You can specify them in yaml,
   but cannot change them yet.
4. Setting "options" to the operations (e.g., write concern)
5. Much more. 

Dependencies
------------
* The [C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master)
* [yaml-cpp](https://github.com/jbeder/yaml-cpp)
* A compiler that supports c++11
* [cmake](http://www.cmake.org/) for building
* [Boost](http://www.boost.org/) for logging. Version 1.48 or later

Building
--------

    mkdir build
    cd build
    cmake ..
    make

Examples
--------

There are a collections of examples in the examples directory. To run
the forN.yml example, simply do:
    ./build/mwg examples/forN.yml

The workloads are all specified in yaml. An example find node is:

        name : find
        type : find
        filter : { x : a }
        next : sleep

The main parts of this are:
* name: This is a label used to refer to the node or operation
* type: This says what the operation should be. Basic operations
  include the operations supported by the C++11 driver. Currently only
  a limited subset is supported.
* filter: This is specific to the find operation. The argument will be
  converted into bson, and will be used as the find document passed
  to the C++11 driver when generating the find
* next: This is the node to transition to when completing this
  operation.

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
