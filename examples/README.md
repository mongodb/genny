Examples
========

This directory contains a number of example workloads. They all
run. The subdirectory future contains example workloads that use
features not fully operational yet, or don't exist yet.

A Tour
---------

- [operations.yml](operations.yml): This file demonstrates many of the
  supported operations, in a list. It executes each operation once
- [sample1.yml](sample1.yml): Four node example. Inserts a document of one form, and
  then randomly chooses between inserting a document of a different
  form, or doing a query. Here's an [image of the sample1 graph](images/sample1.png).
- [sample2.yml](sample2.yml): Extension of sample1 with the addition of an absorbing
  state. Here's an [image of the sample2 graph](images/sample2.png)
- [sample1+threads.yml](sample1+threads.yml): An extension to sample1.yml to run 10 threads
  of execution instead of 1. 
- [workloadNode.yml](workloadNode.yml): This is a simple example showing that a workload
  can be embedded within another workload. The example has a trivial
  workload, but any workload could be embedded. 
- [forN.yml](forN.yml): This demonstrates a simple loop that executes an
  insert_one operation 5 times. Note that a workload node can be the
  embedded node using the workloadNode from the previous example. 
- [doAll.yml](doAll.yml): Demonstrates simple fork join behavior. A thread is
  started for each childNode, and they execute in parallel until they
  get to the join node. 
- [override.yml](override.yml): We can change values in documents. Increment, random
  int, random string, date. 
