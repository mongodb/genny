Future Features
===============

This is an incomplete list. It can be viewed as a roadmap for the
tool.

Document and value transformations
----------------------------------

1. Generalize the current value transformations in overrideDocument,
   workloadNode, and set_variable into common code.
2. More types of transformation of values and tools for building
   documents (hashing, concatenate, adding fields to an existing document)
3. Use of variables and value generation in any field. Examples:
   1. In a ForN node, should be able to set N to a random integer, or
      from an incrementing variable.
   2. A random_string should be able to have a random length, or a
      length read from a variable.
4. _generator_: Similar to an _override_, only evaluate it once at
   startup
5. General document and value templating for more compact
   definitions. Will build on top of the existing value generation
   functionality.

Access state from an embedded workload
--------------------------------------

Allow an embedded workload to access and update state in the parent
workload. Currently we can pass certain values into the embedded
workload, but it is limited, and there is no return path.

e.g., give 5 copies of an embedded workload unique sequence ids by
incrementing a field

CRUD Operations
---------------

### More CRUD Operations

* Aggregation (Can be done through db command)
* Remaining database operations (can be done through db command)

### Wider support for using the result of an operation

* The operations should return a value that is propogated in the
  thread state. This will feed in with choice nodes, and document
  manipulation. Currently the _count_ and _find\_one_ operations
  propogate their results. All CRUD operations should return
  something, even if it's just an OK.

Additional Control Nodes
------------------------

1. Phase Marker: Make a transition externally visible. For instance,
   to mark the end of the warmup phsae of a workload.
2. External trigger: A node that waits for an external trigger before
   executing it's next node. For multi machine client systems, this could be
   used with the phase marker to synchronize multiple client
   machines. For example, all the clients can have a phase marker node
   to indicate the end of warmup, and then immediately wait on an
   external trigger. External software could track when all the
   clients have finished their warmups, and then indicate for all of
   the clients to continue.
3. Wrapper node: Wrap a set of nodes and collect statistics on it as
   if it was a single node. Start timing when you enter the set (or
   entry point to set?), and stop when it leaves the set.
4. Load File: Ability to load a file of json or bson into the database
   similar to mongoimport and mongorestore functionality.

Statistics
----------

The following statistics and related features are not implemented yet.

1. Histogram data
2. Percentile data (95th, 99th percentile), based on histogram data.

I also intend to do the following:

1. Rewrite the stats to be accumulating, rather than resetting. This
   will allow for easy analysis of different regions of behavior.
2. Add the ability for a phase marker node to trigger a stats
   dump. This will enable specifically measuring stats for different
   phases of execution.

The two points combined will be enable us to gather periodic stats,
phase based stats, and overall stats at the same time. It will require
post-processing to generate useful statistics. A script will be
included to perform the post-processing.
