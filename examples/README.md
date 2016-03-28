Examples
========

This directory contains a number of example workloads. They all
run. The subdirectory future contains example workloads that use
features not fully operational yet, or don't exist yet. This file
provides a tour through some of those scripts, and also a tour of the
basic syntax.

A Tour of the example scripts
-----------------------------

These examples are all trivial workloads that illustrate different
concepts or functionality supported by the tool. There are more
complex examples described further down the page.

- [sample1.yml](sample1.yml): Five node example. Inserts a document of
  one form, sleeps for a second, and
  then randomly chooses between inserting a document of a different
  form, or doing a query. Here's an
  [image of the sample1 graph](images/sample1.png), along with sample
  [console output](sample1.output.txt) and
  [results file](sample1.results.json). The sample output traces the
  execution of the workload, and then prints statistics out. The
  statistics are available in a structured form in the results file.
- [sample2.yml](sample2.yml): Extension of sample1 with the addition of an absorbing
  state. Here's an [image of the sample2 graph](images/sample2.png)
- [sample1+threads.yml](sample1+threads.yml): An extension to sample1.yml to run 10 threads
  of execution instead of 1.
- [operations.yml](operations.yml): This file demonstrates many of the
  supported operations, in a list (not exhaustive). It executes each operation once
- [workloadNode.yml](workloadNode.yml): This is a simple example showing that a workload
  can be embedded within another workload. The example has a trivial
  workload, but any workload could be embedded.
- [forN.yml](forN.yml): This demonstrates a simple loop that executes
  a sequence of two insert_one operations 5 times. Note that a
  workload node could be used and called multiple times also.
- [spawn.yml](spawn.yml): Demonstrates spawning new threads of
  execution. A thread is started for each node in spawn, and the
  current thread continues to its next state.
- [doAll.yml](doAll.yml): Demonstrates simple fork join behavior. A thread is
  started for each childNode, and they execute in parallel until they
  get to the join node. Here's an [image of the doAll.yml graph](images/doAll.png)
- [ifNode.yml](ifNode.yml): Demonstrates simple ifNode behavior. It
  contains two ifNodes. The first evaluates true, and the second
  false. Here's an [image of the ifNode graph](images/ifNode.png)
- [override.yml](override.yml): We can change values in documents. Increment, random
  int, random string, date.

A tour of the syntax
--------------------

The workloads are all specified in yaml. An example to call the "find"
operation is:

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

Here is a  simple workload that does an insert and a find, and randomly
chooses between them:

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

This workload has a name and a list of nodes. This workload will run
forever. After each operation it will make a random choice of whether to
do an insert_one or find next. The workload can be made finite by
adding an absorbing state.

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
stop when it reaches the Finish state. Additionally, we can set a
limit on how long the workload runs by adding the field runLengthMs

    name: simple_workload
    runLengthMs: 10000
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

This workload will now run for at most 10 seconds (10,000 ms).

More Complex Examples
---------------------

The examples directory contains some more advanced examples as well.

### [background_index.yml](background_index.yml)

This workload is based off an FSM workload. It creates an index in the
background while performing a collection of CRUD operations at the
same time. The background index creation and the background workload
are defined as their own workloads, and can be run independently. They
are run in a coordinated fashion in the main workload.

### [simpletext.yml](simpletext.yml)

This workload is a protoype re-implementation of the tests in
mongo-perf simple_test.js. Instead of generating random words and
phrases, it includes words and phrases from an external file. It can
be run two different ways: either loading existing json documents or
generating json documents based on a dictionary of phrases and
terms. The actual data used is toy example data, but the files could
be trivially replaced with real data.

### [text_test.yml](text_test.yml)

This workload reimplements and extends the workload reported in
[SERVER-21690](https://jira.mongodb.org/browse/SERVER-21690). It uses
the load\_file node to import the document corpus used in the
ticket. It creates the text index and runs the aggregation workload
described in the ticket. Additionally, the workload can be run with a
query based workload.

The data must be separately downloaded before running, but
there are instructions within the file.

### [insert\_increasing\_threads.yml](insert_increasing_threads.yml)

This workload is based on
[SERVER-20409](https://jira.mongodb.org/browse/SERVER-20409). It
steadily spawns new threads up to 15,000 threads. Each thread inserts
a simple document and then sleeps for 100 ms. The workload should be
tailored to the machine on which it is running.
