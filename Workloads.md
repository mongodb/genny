Workloads
=========

A workload is a complete specification of a load to be applied to a
server. It consists of a graph of [nodes](Nodes.md).

A workload has the following fields:
* _name_: optional string label for the workload. 
* _nodes_: Sequence of [nodes](Nodes.md) that comprise the workload. 
* _seed_ : The random seed for the random number generator.
* _wvariables_ : A sequence of string, value pairs. The value can be
  anything that can be represented in BSON. For now it can be
  initialized to an int, string, or embedded document or list. These
  are workload wide variables, and are shared between threads with
  proper locking around accesses. Because of the locking, _tvariables_
  should be used when reasonable. 
* _tvariables_ : These are thread level variables, and are identical
  to the _wvariables_, except that every thread gets their own copy of
  these variables, and so no locking is needed around their access.
* _database_ : Optional string value defining the default database to
  use.
* _collection_ : Optional string value defining the default collection
  name to use (does not include database name). 
* _threads_ : Number of parallel threads to use
* _runLength_: (Currently unsupported) Maximum execution time for the
  workload. The execution time can be shorter based on the sequence of
  exeution.

### Future Work

* Enable _runLength_ to actually control the maximum execution time
  for a workload. 
