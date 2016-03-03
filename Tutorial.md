MWG Tutorial
------------

This document suggests a sequence of exercises of increasing
complexity using the mwg tool. It is intended as a practical
introduction to using the tool. Most of the exercises also include a
series of steps of increasing detail. We suggest you go through the
exercises in order, performing the simplest version of each step, and
then adding on embellishments. You should run your workload each time
you change it, and observe the difference in behavior.

1. Read through the [docs](Readme.md) and the
   [examples](examples/Readme.md).
2. Run [sample1.yml](examples/sample1.yml) in the examples
   directory. "bin/mwg examples/sample1.yml". Make sure there is a
   running mongod on the local system on port 27017. 
   1. Compare the output to the expected
      [console output](examples/sample1.output.txt).
   2. Look through the results.json file. Do the results make sense to
      you? Do they surprise you?
3. Type "mwg -h". Try running sample1.yml with different command line
   options.
   1. Change the number of threads
   2. Change the runLength (two ways).
   3. Collect periodic stats using the "-p" flag. Review the results
      in results.json.
   4. Connect to a remote mongod, repl set, or sharded cluster.
4. Make a copy of sample1.yml and make changes to it
   1. Change the documents inserted.
   2. Remove the sleep node
   3. Use an override document to insert random data.
   4. Replace the insert_one or find with a different collection
      operation.
   5. Add a new node reachable through the random choice node.
5. Make a workload that puts a mix of operations against the
   database.
   1. Workload puts a consistent load of each type of operation.
   2. Workload puts a randomly varying load of each type of operation.
   3. The workload has a setup phase before doing the testing
      phase. Consider using a ForN node to insert a specific number of
      documents.
6. Create a workload that composes two workloads. Combine the workload
   from previous step, with a long running operation (e.g.,
   aggregation, index build, ...)
   1. Include a setup phase
   2. The first workload should run for a period of time before the
      long running operation starts.
   3. The first workload should continue for a period of time after
      the long running operation starts.
   4. Can you observe the impact of the long running operation on the
      first workload?
   5. Can you adjust the intensity of the first workload, such that it
      affects the latency of the long running operation?
7. Create a workload with an arrival process
   1. Create a workload that creates a new thread every 100 ms, until
      there are 100 threads. Make the threads do something.
   2. Make a workload in which a new thread does a certain sequence of
      operations (possibly with loops), before finishing.
      1. Can you get the workload to a steady state?
      2. How can you show the workload has reached a steady state?
   
