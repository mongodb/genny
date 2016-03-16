Workload Generation
===================

Quickstart
----------

On Mac

    #Start a mongod on default port
    wget https://s3.amazonaws.com/cap-david-daly/WorkloadGeneration-0.3.0-Darwin.tar.gz
    tar -zxvf WorkloadGeneration-0.3.0-Darwin.tar.gz
    cd WorkloadGeneration-0.3.0-Darwin
    bin/mwg examples/hello_world.yml

Or on linux (compiled on Ubuntu 14.04)

    #Start a mongod on default port
    wget https://s3.amazonaws.com/cap-david-daly/WorkloadGeneration-0.3.0-Linux.tar.gz
    tar -zxvf WorkloadGeneration-0.3.0-Linux.tar.gz
    cd WorkloadGeneration-0.3.0-Linux
    bin/mwg examples/hello_world.yml

Congratulations, you have run your first model. You can check the
[console output](examples/hello_world.output.txt) and the
[results file results.json](examples/hello_world.results.json) and compare
the results to my results.

The workload in [hello_world.yml](examples/hello_world.yml) performs a
single insert_one of the document {x: "Hello World"} into a default
database (testDB) and collection (testCollection). Try opening the
mongo shell, and finding the document:

    use testDB
    db.testCollection.findOne()

The [results.json](examples/hello_world.results.json) file saves
statistics on the workload execution in a structured json document,
matching the structure of the workload. You should be able to see that
the "Hello World" workload ran once (count: 1), and within that
workload the "insert\_one0" node also ran once (count: 1), along with
its mean latency (meanMicros) in microseconds.

"insert\_one0" is generated name for the node since we did not
explicitly name it. We can give it a name by adding the text "name:
Insert_Hello" after the print statement. Try adding the line to your
copy of the file. It should look like this:

    main:
      name: Hello_World
      nodes:
        - type: insert_one
          document: {x: Hello World}
          print: Inserted Hello World document
          name: Hello_World

We can also make the test run in a loop, repeatedly inserting this
same document. By default after executing a node, the system executes
the next node in the list or stops if there is no next node. We can
explicitly tell the system the next node to execute by adding a _next_
field. To make a loop, we would use "next: Hello\_World". This results
in a workload that will run forever. We can put a timelimit in
milliseconds on the workload by adding the workload field
_runLengthMs_. The workload with all three of these changes would look
like this:

    main:
      name: Hello_World
      runLengthMs: 5000
      nodes:
        - type: insert_one
          document: {x: Hello World}
          print: Inserted Hello World document
          name: Hello_World
          next: Hello_World

Try updating your copy of hello_world.yml to match, and observe the
output and result file after running. After running the model, try
changing a model parameter, such as the document inserted, or the
run length.

There is also a [tutorial](Tutorial.md) and collection of [examples](examples).

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


Building
--------

See [BUILD.md](BUILD.md)

Examples
--------

There are a collections of examples in the [examples directory](examples/). To run
the [forN.yml](examples/forN.yml) example, simply do:
    mwg examples/forN.yml

Overview of the Tool
--------------------

See [OVERVIEW.md](OVERVIEW.md)


Future Features
---------------

See [FUTURE.md](FUTURE.md) for a list of planned but not implemented
features.
