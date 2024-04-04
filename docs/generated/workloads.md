# Workload Documenation

## ExamplePhase2
### Owner
@mongodb/product-perf
### Description
Example phase to illustrate how PhaseConfig composition works.



## ContinuousWritesWithExponentialCompactTemplate
### Owner
@10gen/server-security
### Description
With queryable encryption enabled, this workload runs alternating CRUD and compact phases,
where the total number of inserts & updates is increased on every CRUD+Compact cycle in order
to grow the ECOC collection to a size that is at least twice its pre-compaction size in
the previous cycle. This is meant to test how long compaction takes relative to ECOC size.
Parameters:
  Database:             name of encrypted database
  Collection:           name of encrypted collection
  ClientName:           name of encrypted client pool
  Namespace:            namespace of the encrypted collection



## YCSBLikeActorTemplate
### Owner
@10gen/server-security
### Description
Phase definitions for encrypted YCSB-like workloads. This defines the YCSBLikeActor
that emulates the MongoDB YCSB workloads.
Parameters:
  Database:             name of encrypted database
  Collection:           name of encrypted collection
  ClientName:           name of encrypted client pool
  Threads:              number of client threads for the CrudActor
  Filter:               filter document to use for find and update operations
  UpdateDocument:       document used in the $set field of an update operation
  InsertDocument:       document to insert during the load phase
  100ReadRepeat:        how many iterations of the 100% read phase to run per thread
  95Read5UpdateRepeat:  how many iterations of the 95/5 phase to run per thread
  100UpdateRepeat:      how many iterations of the 100% update phase to run per thread
  50Read50UpdateRepeat: how many iterations of the 50/50 phase to run per thread



## YCSBLikeEncryptedTemplate
### Owner
@10gen/server-security
### Description
Template for encryption-enabled workloads that emulate a YCSB workload.
Performs queries on an encrypted field, instead of _id, during the read/update phase.
Parameters:
  Database:             name of encrypted database
  Collection:           name of encrypted collection
  ClientName:           name of encrypted client pool
  Field<n>Value:        value of the nth field (n is 1..8)



## ClusteredCollection
### Owner
@mongodb/server-execution
### Description
Run basic insert and find and delete workload on a collection clustered by {_id: 1} .
Clustered collections are planned to mainly serve operations over the cluster
key such as monotonically increasing inserts, range queries and range deletions.



## CreateIndexPhase
### Owner
@mongodb/server-execution
### Description
This Phase has 2 actors, InsertData and IndexCollection. InsertData inserts documents containing
all types of indexes, and IndexCollection creates indexes for each of them, one at a time.



## MixedMultiDeletes
### Owner
@mongodb/server-execution
### Description
Runs mass deletions over a set of 1KB documents, then mass deletions over a set of larger, 10MB documents.
For each size of document:
  . Populates a collection with documents of size n-bytes, then performs a mass deletion on the collection.
  . Repopulates a collection with documents of size n-bytes, then performs a mass deletion while
    performing concurrent writes on another collection.
Allows for comparing the performance of the DELETE_STAGE vs the BATCHED_DELETE_STAGE, primarily in
terms of deletion throughput, and w:majority latencies of the concurrent writes. Also, allows for
comparisons in performance when mass deletions are performed over different sized documents.
The mass deletion namespace is test.Collection0
The concurrent writes namespace is test.concurrentWritesColl

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, delete, batch, deleteMany, latency 


## TimeSeriesUpdatesAndDeletes
### Owner
@mongodb/server-execution
### Description
Set up 1000 independent sensors which will each have 100 buckets, and each bucket has 100
measurements.

The buckets have the meta field, 'sensorId', with values from 0 to 999, each of which has 100
measurements with timestamps within an hour with 36-second intervals. For example, in a bucket,
the measurements look like:
{t: 2023-01-01T5:00:00, m: 42, ...},
{t: 2023-01-01T5:00:36, m: 42, ...},
...
{t: 2023-01-01T5:59:24, m: 42, ...}



## UpdateWithSecondaryIndexes
### Owner
@mongodb/server-execution
### Description
Runs a workload that updates a large range of documents.
Multiple secondary indexes are present.
Update performed with and without a hint.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, update, latency 


## ValidateCmd
### Owner
@mongodb/server-execution
### Description
This workload inserts ~1GB of documents, creates various indexes on the data, and then runs the
validate command. We created this workload to see the performance benefits of improvements
to the validate command, including background validation.



## Default
### Owner
@mongodb/server-execution
### Description
Configuration for the MultiDeletes workload.



## WithSecondaryIndexes
### Owner
@mongodb/server-execution
### Description
Configuration for MultiDeletes workload. This configuration introduces secondary indexes.



## ConnectionsBuildup
### Owner
@mongodb/product-perf
### Description
These are the phases used to reproduce SERVER-53853: Large buildup of mongos to mongod connections and
low performance with secondaryPreferred reads



## AggregateExpressions
### Owner
@mongodb/query
### Description
This file defines a template to use in aggregation expression performance tests.



## BooleanSimplifier
### Owner
@mongodb/query-optimization
### Description
This workload measures performance of boolean expressions which can be simplified by
the Boolean Simplifier. It is designed to track effectiveness of the simplifier.



## CollScanComplexPredicate
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates of
various shapes (CNF, DNF, and mixed predicates with different levels of nestedness).



## CollScanComplexPredicateQueries
### Owner
@mongodb/query
### Description
Defines complex, randomly generated CNF and DNF queries used in 'CollScanComplexPredicate.yml'.



## CollScanLargeNumberOfFields
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries against a collection containing
documents with a large number of fields.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## CollScanOnMixedDataTypes
### Owner
@mongodb/query
### Description
This workload runs collscan queries on various data types.



## CollScanPredicateSelectivity
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries which include conjunctions where the order of
predicates matters due to selectivity of the predicates. In actors' names, a 'good' case means that the ordering
of predicates defined by actual selectivities matches the ordering defined by heuristic selectivity estimation.
Similarly, an 'indistinguishable' case means that heuristic CE defines the same selectivity for all predicates
in the query. For this workload, the important metrics to look at are OperationThroughput and all latency measurements.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## CollScanProjection
### Owner
@mongodb/query
### Description
This workload runs collscan queries with a large projection on around 20 fields.



## CollScanSimplifiablePredicate
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates
that can be simplified by the optimizer.



## FilterWithComplexLogicalExpression
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries with complex logical
expressions that never match a document in the collection.
Each workload name consists of several parts: '{SyntaxType}{PredicateType}'.
'SyntaxType' can be:
  - 'AggregationExpression' means expressions which can be used inside $expr
  - 'MatchExpression' means operators of the find command match predicate language
'PredicateType' can be:
  - 'DeepPredicate' means query with deeply nested expressions
  - 'WidePredicate' means query where operators have a large number of arguments
  - 'SingletonPredicateWithDeepFieldpaths' means query with a single equality predicate where
    nested fieldpaths like '$a.b.c' are used
  - 'WidePredicateWithDeepFieldpaths' means a wide query where nested fieldpaths like '$a.b.c'
    are used
  - 'MixedPredicate' means query which combines wide and deep types
  - 'TargetPath' and 'MissingPath' mean query which targets a path present only in some documents
  - 'MissingPathSuffix' means query is searching a path whose suffix cannot be found in the
  document

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## GetBsonDate
### Owner
@mongodb/query-execution
### Description
This file defines a parameterized configuration 'GetBsonDate' to work around the issue of
ISODate.

TODO PERF-3132 Use the date generator instead of this workaround.



## GroupStagesOnComputedFields
### Owner
@mongodb/query
### Description
This file defines templates to use in workloads exercising aggregate stages on computed
time fields.



## IDHack
### Owner
@mongodb/query
### Description
Defines common configurations for IDHack query workloads.



## LookupCommands
### Owner
@mongodb/query-execution
### Description
Defines common configurations for 'LookupSBEPushdown' and 'LookupSBEPushdownMisc' workloads.



## MatchFilters
### Owner
@mongodb/query
### Description
This workload tests a set of filters in the match language. The actors below offer basic
performance coverage for said filters.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## Multiplanner
### Owner
@mongodb/query
### Description
This file defines templates to use in multiplanner performance tests.



## RepeatedPathTraversal
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries over a set of paths which
share a common prefix. Crucially, these queries never match a document in the collection.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## RunLargeArithmeticOp
### Owner
@mongodb/query
### Description
This phase template constructs an aggregation pipeline that multiplies together
the provided arguments.



## TimeSeriesLastpoint
### Owner
@mongodb/query
### Description
These are the phases used to measure performance of the last-point-in-time query optimization on timeseries collections.



## TimeSeriesSortCommands
### Owner
@mongodb/query
### Description
These are the phases used to measure performance of the bounded sorter for timeseries collections.



## Views
### Owner
@mongodb/query
### Description
Defines common configurations for workloads that operate on views.



## StartupPhasesTemplate
### Owner
mongodb/server-replication"
### Description
Common definitions to support the workloads in replication/startup.



## DesignDocWorkloadPhases
### Owner
STM
### Description
Common definitions to support the workloads in scale/LargeScale. See
LargeScaleSerial.yml for a general overview of what the large-scale workloads are
testing.

Note that there aren't any performance targets defined - these were moved to an
out-of-band tracking system that automatically spots regressions across all recorded
measurements.



## LargeScalePhases
### Owner
Storage Engines
### Description
This is the set of shared phases for the Large Scale Workload Automation project.

Since the LSWA workloads contain common phases, they've been separated in this phase file and can
be included in each workload as needed via the `ExternalPhaseConfig` functionality.



## MixPhases
### Owner
@mongodb/product-perf
### Description
Phase defintions for the MixedWorkloadsGenny, which is a port of the mixed_workloads in the
workloads repo. https://github.com/10gen/workloads/blob/master/workloads/mix.js. It runs 4 sets of
operations, each with dedicated actors/threads. The 4 operations are insertOne, findOne,
updateOne, and deleteOne. Since each type of operation runs in a dedicated thread it enables
interesting behavior, such as reads getting faster because of a write regression, or reads being
starved by writes. The origin of the test was as a reproduction for BF-2385 in which reads were
starved out by writes.

  
### Keywords
scale, insertOne, insert, findOne, find, updateOne, update, deleteOne, delete 


## SetClusterParameterTemplate
### Owner
@mongodb/sharding
### Description
Template for setting a cluster parameter.



## ShardCollectionTemplate
### Owner
@mongodb/sharding
### Description
Template for sharding a collection.



## MultiUpdatesTemplate
### Owner
@mongodb/sharding
### Description
This workload template does the following (with
PauseMigrationsDuringMultiUpdates disabled):
  * Create an unsharded collection.
  * Insert 50k documents of around 20kB each using the monotonic loader.
  * Run updateMany as many times as possible in two 5 minute phases, where
    updateMany will update all of the 50k documents per command in the first
    phase and only a single document per command in the second phase.

Two parameters are accepted to alter the behavior of this workload:
  * ShardCollectionPhases: Valid values are [] and [1]. If set to []
    (default), the collection remains unsharded. If set to [1], the collection
    will be sharded with a hashed shard key on _id.
  * PauseMigrationsPhases: Valid values are [] and [1]. If set to []
    (default), PauseMigrationsDuringMultiUpdates will be disabled. If set to
    [1], PauseMigrationsDuringMultiUpdates will be enabled.

The intent of this workload is to measure the throughput of updateMany, so the
OperationThroughput and OperationsTotal metrics for DocumentUpdater.UpdateAll
and DocumentUpdater.UpdateOne should be used to evaluate performance when
updating all documents per command and a single document per command
respectively.

  
### Keywords
AdminCommand, MonotonicSingleLoader, CrudActor, updateMany, sharding 


## AvgAcctBal
### Owner
@mongodb/product-query
### Description
Run an internal TPC-H query to get the average customer account balance against the denormalized
schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## AvgItemCost
### Owner
@mongodb/product-query
### Description
Run an internal TPC-H query to get the average cost of item sold against the denormalized schema
for scale 10. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## BiggestOrders
### Owner
@mongodb/product-query
### Description
Run an internal TPC-H query to get the biggest EUROPE orders against the denormalized schema for
scale 10. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 gainst the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## TotalLineitemRevenue
### Owner
@mongodb/product-query
### Description
Run an internal TPC-H query to sum up total price across all lineitems against the denormalized
schema for scale 10. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## TotalOrderRevenue
### Owner
@mongodb/product-query
### Description
Run an internal TPC-H query to sum up total price across all orders against the denormalized
schema for scale 10. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.



## ChangeEventApplication
### Owner
@mongodb/product-perf
### Description
This workload is a mixed workload to test the maximum throughput that
can be achieved by change event application (CEA). The workload starts Mongosync
on a cluster with an empty initial dataset, waits for it to transition to
CEA and then starts inserting documents on the source cluster.

  
### Keywords
c2c, replication, cluster to cluster sync, change event application, CEA 


## CollectionCopy
### Owner
@mongodb/product-perf
### Description
This workload is a short version load used to test Mongosync Collection Copy stage performance.
The workload starts Mongosync on a cluster with an preloaded initial dataset.

  
### Keywords
c2c, replication, collection copy, cluster to cluster sync 


## MongosyncScripts
### Owner
@mongodb/product-perf
### Description
This is a collection of helper scripts used to communicate with mongosync. To use them,
use LoadConfig and load the script you would like to use in your actor's phase.
For example:
- Name: StartMongosync
  Type: ExternalScriptRunner
  Threads: 1
  Phases:
    - LoadConfig:
      Path: ./MongosyncScripts.yml
      Key: StartMongosync



## eMRCfBench
### Owner
Storage Engines
### Description
Test workload to evaluate Storage Engine behavior when running in a
degraded replica set with or without eMRCf enabled.  A "degraded" replica
set here means one without an active majority of data bearing nodes.  I.e.,
a PSA set with the Secondary offline.

Currently the workload is split across several yml files because we need to script some things
between the different parts. This is the benchmark phase. We run it both before and after growing
the WT history store so we can evaluate the effect of a large history store. The benchmark has
four phases. Each phase performs one million operations with different request mixes:

100% Inserts
50% Updates, 50% Reads
10% Updates, 90% Reads
100% deletes



## eMRCfGrow
### Owner
Storage Engines
### Description
Test workload to evaluate Storage Engine behavior when running in a
degraded replica set with or without eMRCf enabled.  A "degraded" replica
set here means one without an active majority of data bearing nodes.  I.e.,
a PSA set with the Secondary offline.

Currently the workload is split across several yml files because we need to script
some things between different parts.  This is the grow phase.  It churns our dataset
with a lot of updates, inserts, and deletes.  It intent is to keep the logical size
of the data set the same (i.e., same number of documents with the same large/small mix)
but to cause the WT history store to grow when we run it on a degraded PSA replica set.



## eMRCfPopulate
### Owner
Storage Engines
### Description
Test workload to evaluate Storage Engine behavior when running in a
degraded replica set with or without eMRCf enabled.  A "degraded" replica
set here means one without an active majority of data bearing nodes.  I.e.,
a PSA set with the Secondary offline.

Currently the workload is split across several yml files because we need to script
some things between different parts.  This is the population phase.  It starts with
an empty database and populates it with an initial set of documents.  We create a mix
of small (50-200 byte) and large (200-1000 byte) documents in a 10:1 ratio.



## maps_medical
### Owner
@10gen/server-security
### Description
Models the QE acceptance criteria workload.



## patchConfig
### Owner
@10gen/server-security
### Description
This workload evaluates the performance of Queryable Encryption against the established “Queryable Encryption Performance Release Criteria.” 



## ChooseFromDataset
### Owner
@10gen/product-perf
### Description
Demonstrate the ChooseFromDataset generator. The ChooseFromDataset generator takes one single
argument, a path to a file. It then returns random lines from that file. The file should exist
and shouldn't be empty. As an example you can use familynames.txt, names.txt and airports_codes.txt.
If you specify a relative path the pathfile will depend on your current working directory (cwd).
If running in evergreen, the relative path needs to be: ./src/genny/src/workloads/datasets/.
If running locally, the relative path needs to be: ./src/workloads/datasets/.



## CollectionScanner
### Owner
Storage Engines
### Description
A workload to test/document the collection scanner actor which is used to scan collection in a
given database. It takes numerous configuration options to adjust its behaviour.



## CrudActor
### Owner
@10gen/dev-prod-tips
### Description
This is a demonstration of the CrudActor. It performs writes, updates, and drops
to demonstrate the actor.



## CrudActorAggregate
### Owner
@mongodb/query
### Description
This workload demonstrates using the CrudActor to run an aggregate command. Like find operations,
genny will exhaust the cursor, measuring the initial aggregate command and all subsequent getMore
operations.



## CrudActorEncrypted
### Owner
@10gen/server-security
### Description
Example workload demonstrating encryption support in Genny's client pools.
The CrudActor in this workload performs encrypted operations using a Client
that is configured with the FLE and Queryable auto encryption options.

Encryption is configured by first defining the schemas of each encrypted collection
under the Encryption.EncryptedCollections node. This node is a sequence of collection
descriptors that each must define the following fields:

  Database           - the name of the encrypted database
  Collection         - the name of the encrypted collection
  EncryptionType     - the type of client-side encryption used (either 'fle' or 'queryable')

Each entry in this sequence must have a unique combination of Database and Collection names.
In other words, each collection must have a unique namespace. This is to prevent clients from
defining two different schemas for an encrypted namespace on the same URI.

Depending on the EncryptionType, the encryption schema may be specified using either:
  FLEEncryptedFields        - if EncryptionType is "fle"
  QueryableEncryptedFields  - if EncryptionType is "queryable"

These map the path of an encrypted field in the collection to its encryption parameters.
Both can appear under a collection descriptor node, but only the node that matches the specified
EncryptionType will apply throughout the workload.

Each subnode of FLEEncryptedFields or QueryableEncryptedFields is a key-value pair where
the key is the field path, and the value is an object whose required fields depend on the type.

Field paths cannot be a prefix of another path under the same FLEEncryptedFields or
QueryableEncryptedFields map. For instance, "pii" and "pii.ssn" are not allowed, since "pii" is
a path prefix of the other.

Under FLEEncryptedFields, each object mapped to a path must have the following fields:
  type      - the BSON type of values for this field
  algorithm - the encryption algorithm: either "random" or "deterministic"
              "deterministic" means that the ciphertext is always the same for the same
              plaintext value, which means that this field can be used on queries.

Under QueryableEncryptedFields, each object mapped to a path must have the following fields:
  type      - the BSON type of values for this field
  queries   - an array of objects that specify "queryType" and other parameters for QE.

To enable encryption in each Client, provide an EncryptionOptions node with the following
fields:

  KeyVaultDatabase     - the name of the keyvault database
  KeyVaultCollection   - the name of the keyvault collection
  EncryptedCollections - sequence of encrypted collection namespace strings

The EncryptedCollections subnode is a set of namespaces that must have a corresponding
descriptor under the top-level EncryptedCollections node.

An Actor that wishes to perform encrypted operations on an encrypted collection must
be using a Client pool with an EncryptedCollections subnode that includes the collection's
namespace.

During client pool setup, key vaults in each unique URI are dropped & created once, when the
first client pool for that URI is created.  Data keys for encrypted namespaces are generated
only once, if the associated key vault & URI does not yet contain keys for that namespace.
This means that if two client pools have a similar URI and key vault namespace, then the
encrypted collections they have in common will be using the same data keys.

By default, encrypted CRUD operations require a mongocryptd daemon to be running and listening
on localhost:27020. An alternative option is to set Encryption.UseCryptSharedLib to true, and to
provide the path to the mongo_crypt_v1.so shared library file using Encryption.CryptSharedLibPath.



## CrudActorFSM
### Owner
@mongodb/product-perf
### Description
Example workload demonstrating FSM support in CrudActor. The CrudActor supports finite state
machines, with states and transitions.

Each state specifies a list of transitions. The next transition is picked probabilistically, based
on the transition weights. Transitions may experience delay (sleepBefore), before changing the
state. The state has associated operations which execute while the FSM remains in that
state.

By default the system will measure the latency to execute each set of operations associated with a
state, and each individual operation.

There is an initial state vector, with weights. Each actor instance will probabilistically and
independently choose its initial state based on the weights.

This example workload is a toy example modeling the state of 5 smart phones. The model has four
states for each phone: On, Off, Sleep, and Error, and transitions between those states. In this
example, there is monitoring for the smart phones, and the database gets updated based on the
state of the phones and state changes. The workload (monitoring app) tracks the current phone
state in one document per phone (update operations). Note that this is separate than the state
machinery tracking of state.



## CrudActorFSMAdvanced
### Owner
@mongodb/product-perf
### Description
This example extends CrudActorFSM.yml, with the addition of operations associated with
transitions, in addition to states. We have not implemented this functionality at this point, nor
have explicit plans to implement. As such, this example should be considered suggestive of future
functionality, but not prescriptive. We include this example definition in case someone wants the
functionality at some future point and to guide discussion. It uses the CrudActor to implement a
FSM. Choices are declared with weights that are converted into probabilities.

TODO: Remove this file from skipped dry run list when implemented
(src/lamplib/src/genny/tasks/dry_run.py)

There is an initial state vector, with weight. Each actor instance will probabilistically and
independently choose it's initial state based on the weights.

Each state specifies a list of transitions. The next transition is picked probabilistically, based
on the transition weights. Transitions may experience delay (sleepBefore, sleepAfter) before or
after executing a (possibly empty) set of operations, before transitioning to the next state. The
state itself may execute its own set of operations.

By default the system will measure the latency to execute each set of operations (per transition
or per state), and each operation. The latencies associated with each transition and state are
tracked separately.

This example workload is a toy example modeling the state of 5 smart phones. The model has four
states for each phone: On, Off, Sleep, and Error, and transitions between those states. In this
example, there is monitoring for the smart phones, and the database gets updated based on the
state of the phones and state changes. The workload (monitoring app) tracks the current phone state in
one document (update operations). Note that this is separate than the state machinery tracking of
state. The workload also counts the number of transitions from state On to each of
the other three states (also update operations).



## CrudActorTransaction
### Owner
@10gen/dev-prod-tips
### Description
This workload provides an example of using the CrudActor with a transaction. The
behavior is largely the same, nesting operations inside the transaction block.



## CrudFSMTrivial
### Owner
@mongodb/product-perf
### Description
Trivial tests of CRUD FSM functionality without timing. See CrudActorFSM.yml for documentation on
the functionality. This has 3 active states and one absorbing state. It inserts a document every
time it enters a state, and executes 10 transitions of each state machine. All threads start in
one of the first two states. You can track the progress by the inserted documents. Each thread
includes it's ActorId in the documents.



## Deleter
### Owner
Storage Engines
### Description
A workload to test/document the HotDeleter actor which performs a find_one_and_delete per
iteration.



## ExternalScriptActor
### Owner
@mongodb/query
### Description
This workload was created to test an external script runner as per PERF-3198.
The execution stats of the script will be collected with metrics name "ExternalScript"
If the script writes and only writes an integer to stdout as result, the result will be collected to the specified metrics name (DefaultMetricsName as default)



## Generators
### Owner
@10gen/dev-prod-tips
### Description
This workload exhibits various generators, performing insertions to show them off.
Follow the inline commentary to learn more about them.



## GeneratorsSeeded
### Owner
@10gen/dev-prod-tips
### Description
This workload demonstrates the usage of RandomSeed. Genny uses a PRNG and defaults to the
same seed (see RNG_SEED_BASE from context.cpp). As a result every execution of the same workload
file will generate the same random number stream and by extension the same stream of documents.

To generate a different stream of documents, set the RandomSeed attribute. This example shares
the same base workload Generators.yml and varies the RandomSeed and database name.



## HelloWorld-ActorTemplate
### Owner
@10gen/dev-prod-tips
### Description
This workload shows off the actor template utility, which can be used to create a general
actor template which can then be instantiated with parameters substituted.



## HelloWorld-LoadConfig
### Owner
@10gen/dev-prod-tips
### Description
This workload demonstrates the general workload substitution utility. You can use "LoadConfig"
to load anything, even other workloads.



## HelloWorld-MultiplePhases
### Owner
@10gen/dev-prod-tips
### Description

This workload illustrates how to think of Phases happening
simultaneously and how to configure Repeat and Duration
when you have more than one Actor in a Workload.

Start by quickly looking at the actual workload yaml below

Although we write workloads by listing each Actor and then
listing each Phase for that Actor, Genny workloads actually
operate in order of each Phase. We construct each Actor and
then start Phase 0. We wait until all Actors say that they
are done with Phase 0 and then we move on to Phase 1, etc.
Actors using PhaseLoop automatically do this.

It may help to think of the Actor yamls as being written in
parallel rather than in sequence

    Actors:
    - Type: HelloWorld             - Type: HelloWorld
      Name: A                        Name: B
      Threads: 10                    Threads: 2
      Phases:                        Phases:

      - Message: A.0                 - Message: B.0
        Duration: 2 seconds            Repeat: 10

      - Message: A.1                 - Message: B.1
        Blocking: None                 Repeat: 1e3

The HelloWorld Actor echoes its "Message" parameter in a
loop.

There will be 10 threads running HelloWorld with name "A"
and 2 threads running with name "B".

== Phase 0 ==

          A                          B
      Message: A.0              Message: B.0
      Duration: 2 seconds       Repeat: 10

The first phase (Phase 0) starts and all 12 threads (10 from
A and 2 from B) start echoing their respective messages. At
each iteration, the PhaseLoop checks to see if it should
iterate again.

    Actor A is configured to run its iteration in Phase 0
    for 2 seconds, so each of its 10 threads will loop and
    echo "A.0" for 2 seconds.

    Actor B is configured to run its iteration in Phase 1
    only 10 times. So it will echo "B.0" 10 times per thread.
    Almost certainly this will take less than 2 seconds (the
    amount of time Actor A requires to complete Phase 0). In
    this case, Actor B is "done" with its actions in the
    current Phase so its 2 threads will wait and do nothing
    until the next Phase. Compare this with what Actor A does
    in Phase 1 described next.

== Phase 1 ==

After Phase 0 comes Phase 1.

        A                  B
    Message: A.1      Message: B.1
    Blocking: None    Repeat: 1e3

Notice how Actor A does not specify either a Repeat nor a
Duration. This means A is considered a "background" Actor
for this Phase. Since it doesn't specify Repeat or Duration
it must specify Blocking: None as a precaution against
footgunning. It will run as long as it can while other
Actors are doing their work. This is useful if you want to
have some "background" operation such as doing a db.ping()
or a collection.drop() etc.

Actor B's 2 threads will echo "B.1" 1e3 (1000) times each
and Actor A's 10 threads will echo "A.1" for however long
it takes B to do this.

== Potential Gotchas ==

1. You can specify both `Repeat` and `Duration` for a Phase.
   The larger of the two wins. If you specify `Repeat: 1e40`
   and `Duration: 1ms` it will repeat 1e40 times even if it
   takes much longer than 1ms.

2. It is undefined behavior if no Actors specify a Duration
   or Repeat for a Phase. It does not currently cause an
   infinite-loop, but we don't check for this situation
   (which is almost certainly a mis-configuration) at setup
   time.



## HelloWorld
### Owner
@10gen/dev-prod-tips
### Description
This is an introductory workload that shows how to write a workload in Genny.
This workload writes a few messages to the screen.



## HotCollectionWriter
### Owner
Storage Engines
### Description
A basic workload to test/document the HotCollectionUpdater actor type, which adds documents to a
designated "hot" collection.



## HotDocumentWriter
### Owner
Storage Engines
### Description
A workload to test/document the HotDocumentUpdater actor which updates a specified document in a
specified collection.



## InsertWithNop
### Owner
@mongodb/product-perf
### Description
Demonstrate the InsertRemove actor. The InsertRemove actor is a simple actor that inserts and then
removes the same document from a collection in a loop. Each instance of the actor uses a different
document, indexed by an integer _id field. The actor records the latency of each insert and each
remove.

  
### Keywords
InsertRemove, docs 


## Loader
### Owner
@mongodb/product-perf
### Description
The Loader actor loads a fixed number of documents using a fixed
number of threads to a set of collections. The number of collections created
depends on the Phase.MultipleThreadsPerCollection, Phase.CollectionCount,
Phase.Threads and Actor.Threads values and is explained later.

Actor.Threads specifies the total number of threads created and used by the loader.

Phase.MultipleThreadsPerCollection controls whether there is a collections per thread or a
threads per collection load (defaults to false, so at most one thread will write to each collection).

When MultipleThreadsPerCollection is false, the actor will use at most one thread to load
one of more collections.
In this mode of operation the loader Actor will:
  * create a total of  math.floor(Phase.CollectionCount / Phase.Threads) *  Actor.Threads collections.
  * create Actor.Threads loader threads.
  * each loader thread will create math.floor(Phase.CollectionCount / Phase.Threads) collections.
  Both Actor.Threads and Phase.Threads must be set in this mode.

  When MultipleThreadsPerCollection is true, the actor will use at least one thread to load
  each collection.
  In this mode of operation the loader will:
    * creates a total of Phase.CollectionCount collections.
    * each collection will be populated by (Actor.Thread / Phase.CollectionCount) threads.
    * raise an InvalidConfiguration exception if Phase.CollectionCount doesn't divide
    evenly into Actor.Threads.
    * raise an InvalidConfiguration exception if Phase.Threads is set.

  
### Keywords
docs, loader 


## LoggingActorExample
### Owner
@10gen/dev-prod-tips
### Description
The LoggingActor exists so DSI and Evergreen don't quit
your workload for not outputting anything to stdout.
(They do this in case your workload has timed-out.) If
you don't want to add periodic logging to your Actor
(you probably don't because logging would be done by
every thread and would probably make your workload super
verbose), then you can drop this Actor at the end of
your Workload YAML and configure this Actor to log
something periodically in Phases that are likely to run
longer than a few minutes.

This workload "works", but it basically just exits right
away without actually logging anything because the
LoggingActor insists on being a "background" Actor that
can never block a Phase from completing, and it only
actually logs every 10000 iterations.

To use this Actor, copy/paste the below block into
your workload YAML and create a Phase block for
every Phase that may last longer than a few minutes.



## LongLivedCreator
### Owner
Storage Engines
### Description
Basic workload to test/document the long lived creator actor which utilizes the loader actor to
create and populate a set of long lived collections.



## LongLivedReader
### Owner
Storage Engines
### Description
Basic workload to test/document the long lived reader actor, which utilizes the multi collection
query actor under the hood. This actor reads a set of long lived collections over a period of
time.



## LongLivedWriter
### Owner
Storage Engines
### Description
Basic workload to test/document the long lived writer actor, which updates documents in a set of
long lived collections.



## MonotonicSingleLoader
### Owner
@mongodb/sharding
### Description
Loads a large set of documents with a random value assinged to `field`.

  
### Keywords
insert 


## MoveRandomChunkToRandomShard
### Owner
@mongodb/sharding
### Description
Shards a test collection and does a random move chunk.

  
### Keywords
sharding, moveChunk 


## ParallelInsert
### Owner
@mongodb/repl
### Description
TODO: TIG-3321



## QuiesceActor
### Owner
@10gen/dev-prod-tips
### Description
This workload demonstrates the quiesce actor, used to ensure stable
database state and reduce noise.



## RandomSampler
### Owner
Storage Engines
### Description
A workload to test/document the random sampler actor which reads random records in a database.
It chooses collections and then documents from that collection using id based lookup. It has a
ependency on the documents in the collections having monotonically increasing _id's



## RollingCollections
### Owner
Storage Engines
### Description
A workload to test/document the RollingCollections actor which has two modes of operation. A
"Setup" operation which creates CollectionWindowSize collections and populates them with
DocumentCount documents and a "Manage" operation which creates and drops a collection per
iteration.

This actor is intended to create a rolling window of collections.



## RunCommand-Simple
### Owner
@10gen/dev-prod-tips
### Description
This workload demonstrates the RunCommand actor, which can be used
to execute a command against the server.



## RunCommand
### Owner
@10gen/dev-prod-tips
### Description
This workload demonstrates the RunCommand and AdminCommand actors, which can be used to
run commands against a target server.



## SamplingLoader
### Owner
@mongodb/query
### Description
This workload demonstrates using the SamplingLoader to expand the contents of a collection. This
actor will take a random sample of a configurable size and re-insert the same documents, with the
option of transforming them with an agg pipeline. It will project out the "_id" field before
re-inserting to avoid duplicate key errors.



## StreamStatsReporter
### Owner
@10gen/atlas-streams
### Description
Generates load against a stream processor and has a stats reporter actor that will
report stats about that stream processor after the insert phase is complete since
the processing is fully async.



## ExponentialCompact
### Owner
@10gen/server-security
### Description
With queryable encryption enabled, this workload runs alternating CRUD and compact phases,
where the total number of inserts & updates is increased on every CRUD+Compact cycle in order
to grow the ECOC collection to a size that is at least twice its pre-compaction size in
the previous cycle. This is meant to test how long compaction takes relative to ECOC size.



## YCSBLikeQueryableEncrypt1Cf16
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt1Cf16.yml and YCSBLikeQueryableEncrypt1Cf16Sharded.yml. It was
merged into a single file when "OnlyRunInInstance: sharded" became an option.



## YCSBLikeQueryableEncrypt1Cf32
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt1Cf32.yml and YCSBLikeQueryableEncrypt1Cf32Sharded.yml. It was
merged into a single file when "OnlyRunInInstance: sharded" became an option.



## YCSBLikeQueryableEncrypt1Cfdefault
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt1Cfdefault.yml and YCSBLikeQueryableEncrypt1CfdefaultSharded.yml.
It was merged into a single file when "OnlyRunInInstance: sharded" became an option.



## YCSBLikeQueryableEncrypt5Cf16
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt5Cf16.yml and YCSBLikeQueryableEncrypt5Cf16Sharded.yml. It was
merged into a single file when "OnlyRunInInstance: sharded" became an option.



## YCSBLikeQueryableEncrypt5Cf32
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt5Cf32.yml and YCSBLikeQueryableEncrypt5Cf32Sharded.yml. It was
merged into a single file when "OnlyRunInInstance: sharded" became an option.



## YCSBLikeQueryableEncrypt5Cfdefault
### Owner
@10gen/server-security
### Description
Mimics a YCSB workload, with queryable encryption enabled. Performs queries on an encrypted
field, instead of _id, during the read/update phase. This workload was originally two separate
files, YCSBLikeQueryableEncrypt5Cfdefault.yml and YCSBLikeQueryableEncrypt5CfdefaultSharded.yml.
It was merged into a single file when "OnlyRunInInstance: sharded" became an option.



## medical_workload-diagnosis-100-0-unencrypted
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-diagnosis-100-0
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-diagnosis-50-50-unencrypted
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-diagnosis-50-50
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-guid-50-50-unencrypted
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-guid-50-50
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-load-unencrypted
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## medical_workload-load
### Owner
@10gen/server-security
### Description
Models the Queryable Encryption acceptance criteria workloads



## BackgroundIndexConstruction
### Owner
@mongodb/server-execution
### Description
This workload tests the write performance impact of creating indexes on a very large collection.
For this workload we perform an index build in parallel with a write workload.

We measure and care about the index creation time and throughput/latency of write operation
(inserts, deletes). Usually we'll see trade-offs in favour of either background index build time
or write operations.

This test synthetically lowers the number of concurrent operations allowed to represent a
saturated server.

  
### Keywords
stress, indexes, InsertRemove 


## BackgroundTTLDeletions
### Owner
@mongodb/server-execution
### Description
This workload tests the impact of background TTL deletions in a heavily modified collection. This
test does not quiesce between phases as we want the TTL deleter to be constantly running.

Previously this test was part of a legacy suite of workloads used to test called `insert_ttl.js`.
This is a port of the test in order to have better analytics with some minor changes to be
more correct.

  
### Keywords
ttl, stress, indexes, insertMany, CrudActor 


## BackgroundValidateCmd
### Owner
@mongodb/server-execution
### Description
This workload tests the performance of a successful background validation of a collection with a
high load. This helps us visualize impact on throughput and latency. This workload has 3 phases.
First, we will create initial test data. Next, we will run a series of CRUD operations without
validation in the background. Finally, we run a series of CRUD operations with background
validation concurrently.



## ClusteredCollection
### Owner
@mongodb/server-execution
### Description
Run basic workload on a collection clustered by {_id: 1}.

  
### Keywords
indexes, clustered 


## ClusteredCollectionLargeRecordIds
### Owner
@mongodb/server-execution
### Description
Run basic workload on a collection clustered by {_id: 1} with large RecordId's (~130 bytes).

  
### Keywords
indexes, clustered 


## CreateBigIndex
### Owner
@mongodb/server-execution
### Description
This workload tests the performance of index creation on replica sets, standalones, and sharded
clusters. We use 2 main actors, InsertData and IndexCollection. For more information about these
actors, please refer to CreateIndexPhase.yml. Additionally, in sharded instances only, the
EnableSharding and ShardCollection actors are used. This workload specifically also tests the
performance of creating indexes on a collection large enough that the external sorter must spill
to disk. This can happen when the keys are larger than maxIndexBuildMemoryUsageMegabytes. Note
that this test was originally two separate files, CreateIndex.yml and CreateBigIndex.yml,
but was merged into one as part of PERF-3574. CreateIndex.yml itself was originally CreateIndex.yml
and CreateIndexSharded.yml, but was merged into one as part of PERF-4347.

  
### Keywords
indexes, sharding 


## MixedMultiDeletesBatched
### Owner
@mongodb/server-execution
### Description
Deletes a range of documents using the BATCHED_DELETE query exec stage both in isolation and while performing writes on another collection. Tests deletes on documents of size ~1KB then deletes on documents of ~10MB.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, delete, batch, deleteMany, latency 


## MixedMultiDeletesBatchedWithSecondaryIndexes
### Owner
@mongodb/server-execution
### Description
Deletes a range of documents using the BATCHED_DELETE query exec stage both in isolation and while
performing writes on another collection. Introduces secondary indexes on the collection where the
mass deletion is issued to measure the impact of additional work per document deletion on
concurrent write latency. Tests deletes on documents of size ~1KB then deletes on documents of
~10MB.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, delete, batch, deleteMany, latency 


## MixedMultiDeletesDocByDoc
### Owner
@mongodb/server-execution
### Description
Deletes a range of documents using the DELETE query exec stage both in isolation and while
performing writes on another collection. Tests deletes on documents of size ~1KB then deletes on
documents of ~10MB.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, delete, batch, deleteMany, latency 


## MixedMultiDeletesDocByDocWithSecondaryIndexes
### Owner
@mongodb/server-execution
### Description
Deletes a range of documents using the DELETE query exec stage both in isolation and while
performing writes on another collection. Introduces secondary indexes on the collection where the
mass deletion is issued to measure the impact of additional work per document deletion on
concurrent write latency. Tests deletes on documents of size ~1KB then deletes on documents of
~10MB.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, delete, batch, deleteMany, latency 


## MultiPlanning
### Owner
@mongodb/server-execution
### Description
Create collection with multiple indexes and run queries with different selectivity on different
indexes to test how efficiently multi planning can choose the most selective index.

  
### Keywords
indexes 


## PingCommand
### Owner
@mongodb/product-perf
### Description
This is a simple test that runs ping command on MongoDB to meassure the latency of command dispatch.



## SecondaryReadsGenny
### Owner
mongodb/server-replication"
### Description
Test performance of secondary reads while applying oplogs

### Setup

This workload runs only in a replica set with EXACTLY 3 NODES.
To use it in a replica set with a different number of nodes, adapt the
*number_of_nodes parameter (needs to be >1).

### Test

On an empty replica set, the workload performs the following actions:
- Setup stage:
   - Inserted *initial_nb_docs* documents into both the primary and the
     secondaries. This should be done before any writes and reads.
- Actual test stage:
   - *nWriters* background writers doing infinite writes on the primary which
     will replicate these writes to secondaries.
   - 1/16/32 readers doing queries on a specified seconary. Each query has
     *batch_size* numbers of documents and only reads documents inserted in the
     setup stage in order to have more constant behavior all the time.
   - Finally, a query with WriteConcern 3 is used to measure the replication lag
- Cleanup stage:
  - Drop the collection to reset the db SortPattern:
The test cycles through these stages for 1, 16, and 32 reader threads.

### Phases

- 0: LoadInitialData
- 1: Quiesce (fsync, etc)
- 2: WritesOnPrimary (16 Threads) + ReadsOnSecondary (1 Thread)
- 3: ReplicationLag (measure replication lag via write with w:3)
- 4: CleanUp (drop collection)
- 5: LoadInitialData
- 6: Quiesce
- 7: WritesOnPrimary (16 Threads) + ReadsOnSecondary (16 Threads)
- 8: ReplicationLag
- 9: CleanUp
- 10: LoadInitialData
- 11: Quiesce
- 12: WritesOnPrimary (16 Threads) + ReadsOnSecondary (32 Threads)
- 13: Replication lag



## SinusoidalReadWrites
### Owner
@mongodb/server-execution
### Description
This workload attempts to create a sinusoidal distribution of reads/writes to test
the behavior of execution control when the load on the system changes over time.

We do this with two actors, one for writes and one for reads and spawn 500 client
threads. Though the actors run concurrently, we use the sleep functionality
to effectively stop work on one actor while the other reaches its peak. We then
alternate for a couple repetitions.

In this workload, we try to ensure that reads and writes do not overlap much.

  
### Keywords
sinusoidal, execution control, insertMany, find, CrudActor 


## TimeSeriesArbitraryUpdate
### Owner
@mongodb/server-execution
### Description
This test establishes a baseline for the data correction type of time-series updates.

We have 1000 independent sensors which will each have 100 buckets, and each bucket has 100
measurements.

Then we update a metric field of some measurements, filtered by the '_id' field. This simulates
the use case where users want to correct fields of some measurements.



## TimeSeriesRangeDelete
### Owner
@mongodb/server-execution
### Description
This test establishes a baseline for time-based range deletes on time-series collections.

We have 1000 independent sensors which will each have 100 buckets, and each bucket has 100
measurements.

Then we delete data that spans across three buckets for each series, causing two partial-bucket
deletions and one full-bucket deletion. This tests the use case of data correction where the
application deletes data within some time ranges.



## UpdateWithSecondaryIndexes
### Owner
@mongodb/server-execution
### Description
Updates a large range of documents in the collection.
Multiple secondary indexes are present.
Update performed with and without a hint.

  
### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, update, latency, secondary indexes 


## UserAcquisition
### Owner
@mongodb/security
### Description
Measure user acquisition time on UserCache miss.


## ValidateCmd
### Owner
@mongodb/server-execution
### Description
This workload inserts ~1GB of documents, creates various indexes on the data, and then runs the
validate command. We created this workload to see the performance benefits of improvements
to the validate command, including background validation.

  
### Keywords
RunCommand, Loader, validate 


## ValidateCmdFull
### Owner
@mongodb/server-execution
### Description
This workload inserts ~1GB of documents, creates various indexes on the data, and then runs the
validate command. We created this workload to see the performance benefits of improvements
to the validate command, including background validation.

  
### Keywords
RunCommand, Loader, validate 


## Flushing
### Owner
@mongodb/server-perf
### Description
Run large insertMany's to stress checkpointing, journal flush and dirty memory with {w:majority}.
The workload runs a single CrudActor targeting:
  - 'test' database, configurable via the Database parameter.
  - 'Collection0' collection, configurable via the Collection parameter.
  - 10 threads, configurable via the Threads parameter.
  - 5000 iterations, configurable via the Iterations parameter.
  - 1500 documents, configurable via the Documents parameter.
  - {w:majority} insert options, configurable via the Options parameter.
The default documents have a very simple / flat structure as follows:
  - _id: ObjectId
  - x: a ~1KB fixed string 
  - id: 'User%07d' incrementing field.
There are no additional indexes (just _id).
The important ftdc metrics to look at are:
  - ss opcounters insert
  - ss average latency writes
  - system memory dirty
  - ss wt log log sync time duration
  - ss wt transaction transaction checkpoint currently running

  
### Keywords
Flushing, IO, bulk insert, stress, checkpoint, journal 


## ConnectionPoolStress
### Owner
@mongodb/product-perf
### Description
This workload was created to reproduce issues discovered during sharded stress testing. The workload can cause
low performance, spikes in latency and excessive connection creation (on mongos processes).

The main metrics to monitor are:
  * ErrorsTotal / ErrorRate: The total number of errors and rate of errors encountered by the workload. Networking
    errors are not unexpected in general and they should be recoverable. This work load strives to provide a test to
    allow us to measure the scale of networking errors in a stressful test and prove if the networking becomes more
    stable (with lower total errors and a lower error rate).
  * The Operation latency (more specifically the Latency90thPercentile to Latency99thPercentile metrics)
  * The Operation Throughput
  * "ss connections active": the number of connections.

The workload performs the following steps:
  - Phase 0
    - Upsert a Single Document in the collection
    - set ShardingTaskExecutorPoolMaxSize
    - set ShardingTaskExecutorPoolMinSize
  - Phase 1
    - Execute a find command for GlobalConnectionDuration seconds in GlobalConnectionThreads threads.

  
### Keywords
reproducer, connections, secondaryPreferred, sharding, latency 


## ConnectionsBuildup
### Owner
@mongodb/product-perf
### Description
This workload was created to reproduce SERVER-53853: Large buildup of mongos to mongod connections and
low performance with secondaryPreferred reads. This workload was originally two separate files,
ConnectionsBuildup.yml and ConnectionsBuildupNoSharding.yml. It was merged into a single file when
"OnlyRunInInstance: sharded" became an option.

  
### Keywords
reproducer, connections, secondaryPreferred, sharding 


## MongosLatency
### Owner
@mongodb/product-perf
### Description
This workload was created to reproduce the issue described in SERVER-58997. The goal
is to induce periods of latency in the MongoS processes that are not caused by the MongoD processes.

The reproduction comprises of:
  * A sharded collection of 6000 documents with 4 indexes.
  * The collection is sharded on the _id field.
  * A workphase of 270 threads that runs for 60 minutes. Each CrudActor thread
    executes a mixed workload of 40 interleaved operations (20 Finds and 20 Updates).

The main metrics to monitor are:
  * The Operation Throughput
  * The Operation latency
  * "ss connections active"



## CommitLatency
### Owner
@mongodb/product-perf
### Description
Test latencies for a classic transaction of moving money from one account to another, using
different write concern, read concern, sessions and transactions.  Based on
http://henrikingo.github.io/presentations/Highload%202018%20-%20The%20cost%20of%20MongoDB%20ACID%20transactions%20in%20theory%20and%20practice/index.html#/step-24

  
### Keywords
transactions, sessions, write concern, read concern 


## CommitLatencySingleUpdate
### Owner
@mongodb/product-perf
### Description
Single threaded updates to measure commit latency for various write concern.

  
### Keywords
latency, write concern 


## SecondaryAllowed
### Owner
@mongodb/service-architecture
### Description
This workload runs find operations with 'secondary' readPreference and 'local' readConcern. There
are 100 threads allocated to run the find operations, with a rate limit of 1 thread acting per
microsecond. A stepdown is also initiated during the workload to measure the effects of
secondary reads during an election.



## ServiceArchitectureWorkloads
### Owner
@mongodb/service-architecture
### Description
This workload runs find operations with 'primary' readPreference and 'local' readConcern. There
are 100 threads allocated to run the find operations, with a rate limit of 1 thread acting per
microsecond. A stepdown is also initiated during the workload to measure the effects of primary
reads during an election.



## TransportLayerConnectTiming
### Owner
@mongodb/security
### Description
Invoke replSetTestEgress command on replica set members.
Command synchronously establishes temporary connections between cluster nodes.


## AggregateExpressions
### Owner
@mongodb/query-execution
### Description
This workload measures aggregation expression performance.

While measuring performances, this workload collects numbers
for either the SBE or the classic engine aggregation expression
implementations, depending on environments that it runs on.

Numbers on the 'standalone-all-feature-flags' environment are for
the SBE aggregation expressions and numbers on the 'standalone' environment
for the classic aggregation expressions.

  
### Keywords
aggregate, sbe 


## AggregationsOutput
### Owner
@mongodb/query
### Description
This test exercises both $out and $merge aggregation stages. It does this based on all combinations of the following:
  A. The src collection is sharded or unsharded.
  B. The output collection being sharded or unsharded (only for $merge, as $out does not support sharded output collections).
  C. Different number of matching documents (by using $limit).



## ArrayTraversal
### Owner
@mongodb/query
### Description
This workload stresses path traversal over nested arrays. Crucially, these queries never match
a document in the collection.
Each workload name consists of several parts: '{SyntaxType}{PredicateType}'.
'SyntaxType' can be:
  - 'AggregationExpression' means expressions which can be used inside $expr
  - 'MatchExpression' means operators of the find command match predicate language
'PredicateType' can be:
  - 'NestedArray' means query which targets data that recursively nests arrays
  - 'DeeplyNestedArray' is the same as 'NestedArray', except the arrays are nested twice as deep
  - 'ArrayStressTest' means query is exercising array traversal over 2 path components with a
branching factor of 10 elements
  - 'MissingPathSuffix' means query is searching a path whose suffix cannot be found in the
document

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## BooleanSimplifier
### Owner
@mongodb/query-optimization
### Description
This workload measures performance of boolean expressions which can be simplified by
the Boolean Simplifier. It is designed to track effectiveness of the simplifier.



## BooleanSimplifierSmallDataset
### Owner
@mongodb/query-optimization
### Description
This workload measures performance of boolean expressions which can be simplified by
the Boolean Simplifier. It is designed to track effectiveness of the simplifier.



## CPUCycleMetricsDelete
### Owner
@mongodb/sys-perf
### Description
This workload is designed to insert 10k documents into a single collection and then
perform exactly 10k reads. This is designed to help us calculate CPU cycle metrics
when utilizing the Linux 3-Node ReplSet CPU Cycle Metrics 2023-06 variant



## CPUCycleMetricsFind
### Owner
@mongodb/sys-perf
### Description
This workload is designed to insert 10k documents into a single collection and then
perform exactly 10k reads. This is designed to help us calculate CPU cycle metrics
when utilizing the Linux 3-Node ReplSet CPU Cycle Metrics 2023-06 variant



## CPUCycleMetricsInsert
### Owner
@mongodb/sys-perf
### Description
This workload is designed to insert a document, update it and then immediately delete it
(this is repeated 10k times). This is designed to help us calculate CPU cycle metrics when
utilizing the Linux 3-Node ReplSet CPU Cycle Metrics 2023-06 variant for a mixed workload



## CPUCycleMetricsUpdate
### Owner
@mongodb/sys-perf
### Description
This workload is designed to insert 10k documents into a single collection and then
perform exactly 10k updates. This is designed to help us calculate CPU cycle metrics
when utilizing the Linux 3-Node ReplSet CPU Cycle Metrics 2023-06 variant



## CollScanComplexPredicateLarge
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates of
various shapes against a collection of 1M items.



## CollScanComplexPredicateMedium
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates of
various shapes against a collection of 10K items.



## CollScanComplexPredicateSmall
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates of
various shapes against a collection of 100 items.



## CollScanLargeNumberOfFieldsLarge
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries against a collection of 1M
documents with a large number of fields.



## CollScanLargeNumberOfFieldsMedium
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries against a collection of 10K
documents with a large number of fields.



## CollScanLargeNumberOfFieldsSmall
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries against a collection of 100
documents with a large number of fields.



## CollScanOnMixedDataTypesLarge
### Owner
@mongodb/query
### Description
This workload runs collscan queries on different data type alone. The queries are run against a
collection of 1M documents.



## CollScanOnMixedDataTypesMedium
### Owner
@mongodb/query
### Description
This workload runs collscan queries on different data type alone. The queries are run against a
collection of 10,000 documents.



## CollScanOnMixedDataTypesSmall
### Owner
@mongodb/query
### Description
This workload runs collscan queries on different data type alone. The queries are run against a
collection of 100 documents.



## CollScanPredicateSelectivityLarge
### Owner
@mongodb/query
### Description
This workload tests the performance of conjunctive collection scan queries where the order of
predicates matters due to selectivity of the predicates.



## CollScanPredicateSelectivityMedium
### Owner
@mongodb/query
### Description
This workload tests the performance of conjunctive collection scan queries where the order of
predicates matters due to selectivity of the predicates.



## CollScanPredicateSelectivitySmall
### Owner
@mongodb/query
### Description
This workload tests the performance of conjunctive collection scan queries where the order of
predicates matters due to selectivity of the predicates.



## CollScanProjectionLarge
### Owner
@mongodb/query
### Description
This workload runs collscan queries with a large projection on around 20 fields against a
collection of 1M documents.



## CollScanProjectionMedium
### Owner
@mongodb/query
### Description
This workload runs collscan queries with a large projection on around 20 fields against a
collection of 10,000 documents.



## CollScanProjectionSmall
### Owner
@mongodb/query
### Description
This workload runs collscan queries with a large projection on around 20 fields against a
collection of 100 documents.



## CollScanSimplifiablePredicateLarge
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates
that can be simplified by the optimizer against a large collection (1M documents).



## CollScanSimplifiablePredicateMedium
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates
that can be simplified by the optimizer against a collection of 10K documents.



## CollScanSimplifiablePredicateSmall
### Owner
@mongodb/query
### Description
This workload tests the performance of collection scan queries with complex predicates
that can be simplified by the optimizer against a small collection (100 documents).



## CollectionLevelDiagnosticCommands
### Owner
@mongodb/query-execution
### Description
This workload measures performance for diagnostic top command.

  
### Keywords
top, command, admin, stats 


## ColumnStoreIndex
### Owner
@mongodb/query-execution
### Description
This workload compares the query performance with columnar indexes to performances without them.

The workload consists of the following phases and actors:
  0. Warm up cache.
  1. Run queries with columnstore indexes.
  2. Drop columnstore indexes.
  3. Warm up cache again for regular scans.
  4. Run queries without columnstore indexes.

  
### Keywords
columnstore, analytics 


## ConstantFoldArithmetic
### Owner
@mongodb/query
### Description
SERVER-63099 addresses a correctness issue in how we constant fold during multiplication and addition.
This workload tests the impact of the constant folding optimization in the aggregation pipeline to better
understand the impact of SERVER-63099 and hopefully mitigate the performance regression with other optimizations.

The first pipeline is the control and should be unaffected by changes to the associativity of multiplication in the optimizer
because it only contains constants.

The three pipelines in this workload that contain a fieldpath should have different performance characteristics depending
on the level of associativity in the optimizer.

- Full associativity:
  All three pipelines should have similar performance statistics because all 1000 constants are rearranged to be
  folded into one another, ignoring the order of operations in the original pipeline.

- No associativity (simple correctness fix):
  All three pipelines should have similar performance that is significantly worse than full associativity, because
  each document in the pipeline will require 1000 multiplication operations because no constant folding will happen
  with $multiply marked as non-associative.

- Left associativity (implementation of a correct optimization):
  StartingFieldpath should see no regression from the "no associativity" case.
  MiddleFieldpath should see a speedup that puts its performance in between "no associativity" and "full associativity".
  TailingFieldpath should speed up back to its performance in the "full associativity" case.



## CsiFragmentedInsertsFlat
### Owner
@mongodb/query-execution
### Description
This workload compares performance of inserts into a collection with only the default _id index,
and in presence of the columnstore index. It uses an artificial data set with a wide overall
schema and narrow individual objects to model fragmented access to CSI, which clusters entries by
path. The data size is relatively small (1e6 documents yield ~175MB data size and ~105MB storage
size).
We would like to be able to correlate the results of this workload with the similar one that uses
nested data (CsiFragmentedInsertsNested.yml). Please make sure to update both when making changes.

  
### Keywords
columnstore, insert 


## CsiFragmentedInsertsNested
### Owner
@mongodb/query-execution
### Description
This workload compares performance of inserts into a collection with only the default _id index
and in presence of a full columnstore index. We are not comparing to wildcard index because the
nested data makes creating of a wildcard index too slow. Before changing any of the parameters in
this workload please make sure the results can be correlated with 'CsiFragmentedInsertsFlat.yml'.
As the approach in this workload is the same as in 'CsiFragmentedInsertsFlat.yml' with the exception
of data used by the loader (and not comparing to the wildcard index), comments are intentionally
omitted, please refer to the "flat" workload for the details.

  
### Keywords
columnstore, insert 


## CsiHeavyDiskUsage
### Owner
@mongodb/query-execution
### Description
This workload measures bulk insert performance against a collection named "coll" in a database
named "heavy_io". The collection and database must be set up before running the workload. The
workload itself has no expectations as far as the size or schema of the data are concerned but it
is only useful when being run against larger datasets to actually generate heavy disk IO.

Because the workload is targeting large external datasets, it's expected that they would need long
setup time. To avoid setting up twice, we run two, essentially independent, experiments one after
anoter. We don't expect the internal state of WiredTiger to matter as the experiments involve
different indexes, and the inserts into the rowstore itself are ammortized over the large number
of batches in each experiment.

The purpose of this workload is to compare the insert performance in the following two situations:
 - "Default" indexes are present - a set of indexes that are typically present on the target
   dataset, those that would be useful for the typical query workloads. These indexes should be
   created prior to running the workload.
 - Only a column store index is present. A column store index isn't expected to realistically
   replace all other indexes in production, but it could replace some and knowing its relative
   performance cost could help make that decision.

  
### Keywords
columnstore, analytics, scale, insert 


## CumulativeWindows
### Owner
@mongodb/query
### Description
This test exercises the behavior of multiple '$setWindowFields' stages, each with an
["unbounded", "current"] window and a different window function.  Only a single pass over the data
is needed in this case. The workload consists of the following three phases: collection creation,
creation of an index on the timestamp field to avoid generating a sort of the documents and
running of the the cumulative window aggregations.



## CumulativeWindowsMultiAccums
### Owner
@mongodb/query
### Description
This test exercises the behavior of multiple '$setWindowFields' stages, each with an
["unbounded", "current"] window for for multiaccumulators (like
$topN, $firstN, etc) and $top/$bottom which are not present in v5.0. Also moved
$derivative here since it fails with "Exceeded max memory" in v5.0



## DensifyFillCombo
### Owner
@mongodb/query
### Description
This workload tests the performance of queries that combine $densify and $fill stages.
$densify creates new documents to eliminate the gaps in the "timestamp" or "numeric"
fields. The other fields in these generates documents however, will be missing.
$fill set values for the output field (in the context of this workload,
"toFillRandomType" or "toFillNumeric") when the value is null or missing.



## DensifyHours
### Owner
@mongodb/query
### Description
This workload tests the performance of the $densify stage with hour-long step intervals.
The text of each Densify[Unit] workload is identical from from the Actors list to the end of the
file, with all workload-specific details defined as anchors under the GlobalDefaults key. Any
changes to the actors should make use of these anchors and be copy-pasted between the Densify[Unit]
workloads to keep them in sync.
Each densify workload has a metric to cover possible codepaths through the densify stage, namely
different bounds types and whether or not to generate documents along partitions.

Since $densify is a new aggregation stage, there isn't a known "good" runtime for aggregations
that include it. Rather than comparing against a baseline initially, workloads with different
options that generate similar numbers of documents will be compared to ensure that there isn't an
option that relies on a specific codepath that slows down one kind of densification more than another.

Initial investigation has shown that holding other options equal, numeric and millisecond step units
have similar latencies, and all other step units have larger latencies which are all similar to each other.
This is due to the need for date arithmetic to perform extra processing to add an interval of a
certain size to a date. Since dates are stored as milliseconds since the Unix epoch under the hood,
the millisecond step unit can skip most of this processing and just performs the same arithmetic
as the numeric step unit.

The working assumption is that latency will scale linearly with the number of documents generated
by the $densify stage: If N is the number of documents in the collection, and M is the number of
documents generated by $densify, then the theoretical runtime should be O(N+M).

The factors that affect the number of documents generated include, but are not limited to:

- The range that is being densified. This is pretty self-evident, the larger the range that $densify
  is working over, the more documents will be generated. This includes large ranges specified
  explicitly in the bounds option, and implicitly large ranges when densifying over collections
  where the minimum and maximum documents are farther from each other in the
  `bounds: "full"|"partition"` cases.

- The step size. Smaller steps produce more documents when the range is held constant.

- The cardinality of the fields specified in the `partitionByFields`, if set. One document
  will be generated with every value within the range for each partition, so the higher the
  cardinality, the more documents that will be generated. Specifying a `partitionByFields` will
  never decrease the number of documents generated, holding all other options equal.



## DensifyMilliseconds
### Owner
@mongodb/query
### Description
This workload tests the performance of the $densify stage with millisecond step intervals.
The text of each Densify[Unit] workload is identical from from the Actors list to the end of the
file, with all workload-specific details defined as anchors under the GlobalDefaults key. Any
changes to the actors should make use of these anchors and be copy-pasted between the Densify[Unit]
workloads to keep them in sync.

Each densify workload has a metric to cover possible codepaths through the densify stage, namely
different bounds types and whether or not to generate documents along partitions.


Since $densify is a new aggregation stage, there isn't a known "good" runtime for aggregations
that include it. Rather than comparing against a baseline initially, workloads with different
options that generate similar numbers of documents will be compared to ensure that there isn't an
option that relies on a specific codepath that slows down one kind of densification more than another.

Initial investigation has shown that holding other options equal, numeric and millisecond step units
have similar latencies, and all other step units have larger latencies which are all similar to each other.
This is due to the need for date arithmetic to perform extra processing to add an interval of a
certain size to a date. Since dates are stored as milliseconds since the Unix epoch under the hood,
the millisecond step unit can skip most of this processing and just performs the same arithmetic
as the numeric step unit.

The working assumption is that latency will scale linearly with the number of documents generated
by the $densify stage: If N is the number of documents in the collection, and M is the number of
documents generated by $densify, then the theoretical runtime should be O(N+M).

The factors that affect the number of documents generated include, but are not limited to:

- The range that is being densified. This is pretty self-evident, the larger the range that $densify
  is working over, the more documents will be generated. This includes large ranges specified
  explicitly in the bounds option, and implicitly large ranges when densifying over collections
  where the minimum and maximum documents are farther from each other in the
  `bounds: "full"|"partition"` cases.

- The step size. Smaller steps produce more documents when the range is held constant.

- The cardinality of the fields specified in the `partitionByFields`, if set. One document
  will be generated with every value within the range for each partition, so the higher the
  cardinality, the more documents that will be generated. Specifying a `partitionByFields` will
  never decrease the number of documents generated, holding all other options equal.



## DensifyMonths
### Owner
@mongodb/query
### Description
This workload tests the performance of the $densify stage with month-long step intervals.
The text of each Densify[Unit] workload is identical from from the Actors list to the end of the
file, with all workload-specific details defined as anchors under the GlobalDefaults key. Any
changes to the actors should make use of these anchors and be copy-pasted between the Densify[Unit]
workloads to keep them in sync.

Each densify workload has a metric to cover possible codepaths through the densify stage, namely
different bounds types and whether or not to generate documents along partitions.

Since $densify is a new aggregation stage, there isn't a known "good" runtime for aggregations
that include it. Rather than comparing against a baseline initially, workloads with different
options that generate similar numbers of documents will be compared to ensure that there isn't an
option that relies on a specific codepath that slows down one kind of densification more than another.

Initial investigation has shown that holding other options equal, numeric and millisecond step units
have similar latencies, and all other step units have larger latencies which are all similar to each other.
This is due to the need for date arithmetic to perform extra processing to add an interval of a
certain size to a date. Since dates are stored as milliseconds since the Unix epoch under the hood,
the millisecond step unit can skip most of this processing and just performs the same arithmetic
as the numeric step unit.

The working assumption is that latency will scale linearly with the number of documents generated
by the $densify stage: If N is the number of documents in the collection, and M is the number of
documents generated by $densify, then the theoretical runtime should be O(N+M).

The factors that affect the number of documents generated include, but are not limited to:

- The range that is being densified. This is pretty self-evident, the larger the range that $densify
  is working over, the more documents will be generated. This includes large ranges specified
  explicitly in the bounds option, and implicitly large ranges when densifying over collections
  where the minimum and maximum documents are farther from each other in the
  `bounds: "full"|"partition"` cases.

- The step size. Smaller steps produce more documents when the range is held constant.

- The cardinality of the fields specified in the `partitionByFields`, if set. One document
  will be generated with every value within the range for each partition, so the higher the
  cardinality, the more documents that will be generated. Specifying a `partitionByFields` will
  never decrease the number of documents generated, holding all other options equal.



## DensifyNumeric
### Owner
@mongodb/query
### Description
This workload tests the performance of the $densify stage with a numeric step.
The text of each Densify[Unit] workload is identical from from the Actors list to the end of the
file, with all workload-specific details defined as anchors under the GlobalDefaults key. Any
changes to the actors should make use of these anchors and be copy-pasted between the Densify[Unit]
workloads to keep them in sync.

Each densify workload has a metric to cover possible codepaths through the densify stage, namely
different bounds types and whether or not to generate documents along partitions.

Since $densify is a new aggregation stage, there isn't a known "good" runtime for aggregations
that include it. Rather than comparing against a baseline initially, workloads with different
options that generate similar numbers of documents will be compared to ensure that there isn't an
option that relies on a specific codepath that slows down one kind of densification more than another.

Initial investigation has shown that holding other options equal, numeric and millisecond step units
have similar latencies, and all other step units have larger latencies which are all similar to each other.
This is due to the need for date arithmetic to perform extra processing to add an interval of a
certain size to a date. Since dates are stored as milliseconds since the Unix epoch under the hood,
the millisecond step unit can skip most of this processing and just performs the same arithmetic
as the numeric step unit.

The working assumption is that latency will scale linearly with the number of documents generated
by the $densify stage: If N is the number of documents in the collection, and M is the number of
documents generated by $densify, then the theoretical runtime should be O(N+M).

The factors that affect the number of documents generated include, but are not limited to:

- The range that is being densified. This is pretty self-evident, the larger the range that $densify
  is working over, the more documents will be generated. This includes large ranges specified
  explicitly in the bounds option, and implicitly large ranges when densifying over collections
  where the minimum and maximum documents are farther from each other in the
  `bounds: "full"|"partition"` cases.

- The step size. Smaller steps produce more documents when the range is held constant.

- The cardinality of the fields specified in the `partitionByFields`, if set. One document
  will be generated with every value within the range for each partition, so the higher the
  cardinality, the more documents that will be generated. Specifying a `partitionByFields` will
  never decrease the number of documents generated, holding all other options equal.



## DensifyTimeseriesCollection
### Owner
@mongodb/query
### Description
This workload tests the performance of $densify stage with a numeric step
in timeseries collections.



## ExpressiveQueries
### Owner
@mongodb/query
### Description
This workload measures the performance of queries with rich filters. First, we issue a bunch of
plain queries with rich filters to get the performance baseline. After that, we issue the same
queries, but with projection. Finally, we create an index and issue the same queries with
projection, expecting that they will use this index.



## ExternalSort
### Owner
@mongodb/query
### Description
Measures the performance of sort operations that spill to disk. Each benchmarked operation
exercises the external sort algorithm by running a query that sorts a large collection without an
index. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the sort algorithm executes in its entirety.



## FillTimeseriesCollection
### Owner
@mongodb/query
### Description
This workload tests the performance of $fill in timeseries collections.
If there is a nullish value in the output field, $fill replaces
the null value with either the last non-nullish value (in case of $locf)
or the interpolated value (in case of $linearFill).



## FilterWithComplexLogicalExpression
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries with complex logical
expressions that never match a document in the collection.
Each workload name consists of several parts: '{SyntaxType}{PredicateType}'.
'SyntaxType' can be:
  - 'AggregationExpression' means expressions which can be used inside $expr
  - 'MatchExpression' means operators of the find command match predicate language
'PredicateType' can be:
  - 'DeepPredicate' means query with deeply nested expressions
  - 'WidePredicate' means query where operators have a large number of arguments
  - 'SingletonPredicateWithDeepFieldpaths' means query with a single equality predicate where
    nested fieldpaths like '$a.b.c' are used
  - 'WidePredicateWithDeepFieldpaths' means a wide query where nested fieldpaths like '$a.b.c'
    are used
  - 'MixedPredicate' means query which combines wide and deep types
  - 'TargetPath' and 'MissingPath' mean query which targets a path present only in some documents
  - 'MissingPathSuffix' means query is searching a path whose suffix cannot be found in the
  document

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## FilterWithComplexLogicalExpressionMedium
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries with complex logical
expressions that never match a document in the collection.
Each workload name consists of several parts: '{SyntaxType}{PredicateType}'.
'SyntaxType' can be:
  - 'AggregationExpression' means expressions which can be used inside $expr
  - 'MatchExpression' means operators of the find command match predicate language
'PredicateType' can be:
  - 'DeepPredicate' means query with deeply nested expressions
  - 'WidePredicate' means query where operators have a large number of arguments
  - 'SingletonPredicateWithDeepFieldpaths' means query with a single equality predicate where
    nested fieldpaths like '$a.b.c' are used
  - 'WidePredicateWithDeepFieldpaths' means a wide query where nested fieldpaths like '$a.b.c'
    are used
  - 'MixedPredicate' means query which combines wide and deep types
  - 'TargetPath' and 'MissingPath' mean query which targets a path present only in some documents
  - 'MissingPathSuffix' means query is searching a path whose suffix cannot be found in the
  document

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## FilterWithComplexLogicalExpressionSmall
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries with complex logical
expressions that never match a document in the collection.
Each workload name consists of several parts: '{SyntaxType}{PredicateType}'.
'SyntaxType' can be:
  - 'AggregationExpression' means expressions which can be used inside $expr
  - 'MatchExpression' means operators of the find command match predicate language
'PredicateType' can be:
  - 'DeepPredicate' means query with deeply nested expressions
  - 'WidePredicate' means query where operators have a large number of arguments
  - 'SingletonPredicateWithDeepFieldpaths' means query with a single equality predicate where
    nested fieldpaths like '$a.b.c' are used
  - 'WidePredicateWithDeepFieldpaths' means a wide query where nested fieldpaths like '$a.b.c'
    are used
  - 'MixedPredicate' means query which combines wide and deep types
  - 'TargetPath' and 'MissingPath' mean query which targets a path present only in some documents
  - 'MissingPathSuffix' means query is searching a path whose suffix cannot be found in the
  document

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## GraphLookup
### Owner
@mongodb/query
### Description
This test exercises the behavior of $graphLookup where either the local or foreign collection is
sharded. When both collections are unsharded, the pipeline should be moved to the
GraphLookupWithOnlyUnshardedColls workload, so the query can be tested on replica sets as well. We
may want to compare results across the two files, so the loading stage (and some queries) should
be kept as similar as possible.

Note: this workload runs only on sharded clusters.

The workload consists of the following phases:
  1. Creating empty sharded collections distributed across all shards in the cluster.
  2. Populating collections with data.
  3. Fsync.
  4. Running $graphLookups. This includes but is not limited to cases where the top-level
     pipelines are targeted and untargeted, the 'maxDepth' recursion parameter takes on different
     values, and the count of expected matches per input document is varied.



## GraphLookupWithOnlyUnshardedColls
### Owner
@mongodb/query
### Description
This test exercises the behavior of $graphLookup when both the local and foreign collections are
unsharded. When one of the collections is sharded, the pipeline should be moved to the
GraphLookup workload, so that this file can continue to be tested in an unsharded environment. We
may want to compare results across the two files, so the loading stage (and some queries) should
be kept as similar as possible.

Note: this workload runs on replica sets and sharded clusters.

The workload consists of the following phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating collections with data.
  3. Fsync.
  4. Running $graphLookups.



## GroupLikeDistinct
### Owner
@mongodb/query
### Description
This workloads covers $group queries with distinct-like semantics, meaning that only a single document
is selected from each group. Depending on the details of the query and available indexes, the query might
be optimized to use DISTINCT_SCAN plan.

  
### Keywords
Distinct, Group, First, Last, Top, Bottom, timeseries, aggregate 


## GroupSpillToDisk
### Owner
@mongodb/query
### Description
Runs $group queries which are designed to require spilling to disk.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, aggregate, group, spill 


## GroupStagesOnComputedFields
### Owner
@mongodb/query
### Description
The queries in this workload exercise group stage(s) after other stages ($addFields, $match, $sort)
on computed date fields. The queries are motivated by the work on the SBE prefix pushdown project
that enables the execution of $addFields, $match, and $sort in SBE.

  
### Keywords
aggregate, sbe 


## InWithVariedArraySize
### Owner
@mongodb/query-execution
### Description
This workload measures performance for $in for arrays of different sizes. We add the array length
for $in to the plan cache key since there is a possible explodeForSort optimization till we reach
internalQueryMaxScansToExplode that limits the maximum number of explode for sort index scans.
This workload measures performance for $in in both these cases, with and without indexes.

  
### Keywords
in, cache, parametrization, classic, sbe 


## LimitSkip
### Owner
@mongodb/query-execution
### Description
This workload measures performance for $limit and $skip when different constants are supplied as
arguments.

  
### Keywords
limit, skip, cache, parametrization, classic, sbe 


## Lookup
### Owner
@mongodb/query
### Description
This test exercises the behavior of $lookup where either the local or foreign collection is
sharded. When both collections are unsharded, the pipeline should be moved to the
LookupWithOnlyUnshardedColls workload, so the query can be tested on replica sets as well. We may
want to compare results across the two files, so the loading stage (and some queries) should be
kept as similar as possible.

Note: this workload runs only on sharded clusters.

The workload consists of the following phases:
  1. Creating empty sharded collections distributed across all shards in the cluster.
  2. Populating collections with data.
  3. Fsync.
  4. Running $lookups. This includes but is not limited to cases where the top-level pipelines are
     targeted and untargeted, the subpipelines are targeted and untargeted, and the subpipelines
     have cacheable prefixes.



## LookupColocatedData
### Owner
@mongodb/query
### Description
This test exercises the behavior of $lookup when the local and foreign collection data is
co-located. In the queries below, for each local document on each shard, the $lookup subpipeline
is targeted to the same shard.
The workload consists of the following steps:
  A. Creating empty collections, some unsharded and some sharded.
  B. Populating collections with data.
  C. Fsync.
  D. Running $lookups. This includes pipelines using let/pipeline syntax, pipelines with
     local/foreign field syntax, pipelines with different $limits, and pipelines run against
     sharded and unsharded colls.



## LookupNLJ
### Owner
@mongodb/query-execution
### Description
This workload measures $lookup performance for scenarios that look up local scalar/array/dotted
key against foreign collection's scalar/array/dotted key, when the foreign collections have
multiple collection cardinalities such as 1000 / 10000. Note that 100k foreign collection
cardinality will make the testing duration too long, so we skip it.

While measuring performances, this workload collects numbers for either the SBE or the classic
engine $lookup implementations, depending on environments that it runs on. Both SBE and classic
$lookup implementations will be nested loop join for this workload.

Numbers on the 'standalone-sbe' environment are for the SBE $lookup and numbers on the
'standalone' environment for the classic $lookup until the SBE $lookup pushdown feature will
be turned on by default.

In summary, this workload conducts testing for the following test matrix:
- JoinType: NLJ
- ForeignCollectionCardinality: 1000 / 10000
- The local collection has 100k documents and its documents have scalars, arrays and objects.

The workload consists of the following phases and actors:
  0. Cleans up database
     - 'Setup' actor
  1. Loads data into the local and foreign collections
     - 'LoadXXX' actors
  2. Calms down to isolate any trailing impacts from the load phase
     - 'Quiesce' actor
  3. Runs and measures either the SBE NLJ lookups or the classic NLJ lookups, depending on the
     environment
     - 'NLJLookup' actor

  
### Keywords
lookup, aggregate, sbe 


## LookupOnly
### Owner
@mongodb/query
### Description
This test exercises the behavior of a $lookup-only pipeline. We use one "local" collection with
20000 documents {_id, join_val}, and vary the "foreign" collection to test different document
contents as well as different collection sizes. The structure of documents in the foreign
collection is {_id, join_val, <data>}, where <data> is one of the following:
- an integer
- a short string (20 chars)
- a long string (100 chars)
- a small object (~500 bytes)
- a large object (~5,000 bytes)
For each case, we test with collections that have 100, 1000, or 10000 documents

The workload consists of the following phases and actors:
  0. Cleans up database
     - 'Setup' actor
  1. Loads data into the local and foreign collections
     - 'LoadXXX' actors
  2. Calms down to isolate any trailing impacts from the load phase
     - 'Quiesce' actor
  3. Runs and measures the lookup performance
     - 'LookupOnlyXXX' actors

  
### Keywords
lookup, aggregate 


## LookupSBEPushdown
### Owner
@mongodb/query-execution
### Description
This workload measures $lookup performance for scenarios that look up local integer key against
foreign collection's integer key, when the foreign collections have multiple collection
cardinalities such as 1500 / 3500 / 5500 / 7500 / 9500.

While measuring performances, this workload collects numbers for either the SBE or the classic
engine $lookup implementations, depending on environments that it runs on. The SBE $lookup
implements the hash join and the (index) nested loop join but the classic $lookup implements
only (index) nested loop join. So we can compare the SBE HJ / NLJ implementations to the classic
engine NLJ implementation and the SBE INLJ implementation to the classic engine INLJ
implementation.

Numbers on the 'standalone-all-feature-flags' environment are for the SBE $lookup and numbers on
the 'standalone' environment for the classic $lookup until the SBE $lookup pushdown feature will
be turned on by default.

In summary, this workload conducts testing for the following test matrix:
- JoinType: HJ / NLJ / INLJ
- ForeignCollectionCardinality: 1500 / 3500 / 5500 / 7500 / 9500
- The local collection has 100k documents and its documents are small ones

The workload consists of the following phases and actors:
  0. Cleans up database
     - 'Setup' actor
  1. Loads data into the local and foreign collections
     - 'LoadXXX' actors
  2. Calms down to isolate any trailing impacts from the load phase
     - 'Quiesce' actor
  3. Runs and measures either the SBE HJ lookups or the classic NLJ lookups, depending on the
     environment
     - 'HJLookup' actor
  4. Runs and measures either the SBE NLJ lookups or the classic NLJ lookups, depending on the
     environment
     - 'NLJLookup' actor
  5. Builds indexes on foreign collections
     - 'BuildIndexes' actor
  6. Calms down to isolate any trailing impacts from the previous phase
     - 'Quiesce' actor
  7. Runs and measures the SBE INLJ lookups and the classic INLJ lookups
     - 'INLJLookup' actor

  
### Keywords
lookup, aggregate, sbe 


## LookupSBEPushdownINLJMisc
### Owner
@mongodb/query-execution
### Description
This workload conducts testing for the following test matrix:
- LocalCollectionCardinality: 100k / 300k / 500k / 700k
- DocumentSize: Small document size vs Large document size
- JoinFieldType: string type / sub-field type / array type / sub-field array type
- NumberOfLookups: 1 / 2 / 3
- The foreign collection has 1500 documents each of which has a unique key value

But this workload does not try to measure performances for full combination of the above test
dimensions to avoid too lengthy test execution.

While measuring performances, this workload collects numbers for either the SBE INLJ or the
classic engine INLJ $lookup implementations, depending on environments that it runs on. The reason
why it collects numbers for the INLJ lookup implementation is to compare both implementations on
the same algorithmic complexity for fairer comparison and yet to minimize the test execution time
since the INLJ lookup is supposed to be very fast on both the SBE and the classic egnine.

Numbers on the 'standalone-all-feature-flags' environment are for the SBE INLJ lookup and numbers
on the 'standalone' environment for the classic INLJ lookup until the SBE $lookup feature will be
turned on by default.

The workload consists of the following phases and actors:
  0. Cleans up database
     - 'Setup' actor
  1. Loads data into the local and foreign collections.
     - 'LoadXXX' actors.
  2. Builds indexes on the foreign collection.
     - 'BuildIndexes' actor.
  3. Calms down to isolate any trailing impacts from the previous phase
     - 'QuiesceActor'
  4. Runs and measures the miscellaneous lookups.
     - 'INLJLookupMisc' actor

  
### Keywords
lookup, aggregate, sbe 


## LookupUnwind
### Owner
@mongodb/query
### Description
This test exercises the behavior of $lookup when followed by $unwind that immediately unwinds the
lookup result array, which is optimized in the execution engine to avoid materializing this array
as an intermediate step. We use one "local" collection with 20000 documents {_id, join_val}, and
vary the "foreign" collection to test different document contents as well as different collection
sizes. The structure of documents in the foreign collection is {_id, join_val, <data>}, where
<data> is one of the following:
- an integer
- a short string (20 chars)
- a long string (100 chars)
- a small object (~500 bytes)
- a large object (~5,000 bytes)
For each case, we test with collections that have 100, 1000, or 10000 documents

The workload consists of the following phases and actors:
  0. Cleans up database
     - 'Setup' actor
  1. Loads data into the local and foreign collections
     - 'LoadXXX' actors
  2. Calms down to isolate any trailing impacts from the load phase
     - 'Quiesce' actor
  3. Runs and measures the lookup/unwind performance
     - 'LookupUnwindXXX' actors

  
### Keywords
lookup, unwind, aggregate 


## LookupWithOnlyUnshardedColls
### Owner
@mongodb/query
### Description
This test exercises the behavior of $lookup when both the local and foreign collections are
unsharded. When one of the collections is sharded, the pipeline should be moved to the
Lookup workload, so that this file can continue to be tested in an unsharded environment. We may
want to compare results across the two files, so the loading stage (and some queries) should be
kept as similar as possible.

Note: this workload runs on replica sets and sharded clusters.

The workload consists of the following phases:
  1. Populating collections with data.
  2. Fsync.
  3. Running $lookups.



## MatchFilters
### Owner
@mongodb/query
### Description
This workload tests a set of filters in the match language. The actors below offer basic
performance coverage for said filters.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## MatchFiltersMedium
### Owner
@mongodb/query
### Description
This workload tests a set of filters in the match language. The actors below offer basic
performance coverage for said filters.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## MatchFiltersSmall
### Owner
@mongodb/query
### Description
This workload tests a set of filters in the match language. The actors below offer basic
performance coverage for said filters.

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, find 


## MatchWithLargeExpression
### Owner
@mongodb/query
### Description
This workload tests the performance of expression search for parameter re-use during parameterization.
Before September 2023, parameter re-use had O(n^2) complexity due to using a vector for looking up equivalent expressions.
SERVER-79092 fixed this issue by switching over to a map once the amount of expressions reaches a threshold (currently 50).

  
### Keywords
Loader, CrudActor, QuiesceActor, insert, Aggregation, matcher, expressions 


## MetricSecondaryIndexTimeseriesCollection
### Owner
@mongodb/query
### Description
This test exercises the behavior of querying for data in a timeseries collection using a metric index.

The phases are:
0. Create collection
1. Insert events + create index
2. Quiesce
3. Query



## OneMDocCollection_LargeDocIntId
### Owner
@mongodb/query
### Description
This workload tests performance of IDHACK on queries with int _id. Since this workloads loads
very large documents (average 10KB), you may need to free up disk space before running this
workload locally.



## PercentilesAgg
### Owner
@mongodb/query-integration
### Description
Run tests for $percentile accumulator over 2 data distributions. We create a new collection for
each distribution as putting the fields into the same document might be impacted by how much
bson needs to be parsed to access the field but we want to make sure we compare apples to apples.

During development we did not see significant differences in runtime between various data
distributions, and therefore scoped down this test to create a normal distribution and a dataset
created from 5 different distributions.

  
### Keywords
group, percentile 


## PercentilesExpr
### Owner
@mongodb/query-integration
### Description
Run tests for $percentile expression over variously sized input arrays. We create a new collection
for each array size as putting the fields into the same document might be impacted by how much
bson needs to be parsed to access the field but we want to make sure we compare apples to apples.

  
### Keywords
group, percentile 


## PercentilesWindow
### Owner
@mongodb/query-integration
### Description
This tests $percentile window functions over various bounded window sizes. Bounded window
functions use a different implementation (an accurate algorithm) than the $percentile accumulator,
which uses an approximate algorithm. Therefore, we need to measure computing percentiles over
differently sized windows. We test a few cases:
1. Small document-based removable windows
2. Large document-based removable windows
3. Non-removable document-based window (will use an approximate algorithm).
4. Small range-based removable windows
5. Large range-based removable windows
6. Non-removable range-based window (will use an approximate algorithm).

We compare the speed of $percentile over these windows with $minN.
We do not test different percentile values here, since those are tested in a micro-benchmark, and
do not show significant differences.

  
### Keywords
setWindowFields, percentile 


## PercentilesWindowSpillToDisk
### Owner
@mongodb/query-integration
### Description
This tests $percentile window functions with different partitions and indexes to test its behavior
when it spills to disk. There are a few cases to consider
1. Nothing spills.
2. Only the $_internalSetWindowsFields will spill, which in this case would be the $percentile
window function.
3. Both the $sort and $_internalSetWindowsFields spill.
There is a workload in SetWindowFieldsUnbounded.yml that handles the case if only the $sort spills,
and since there is no unique behavior for $percentile in this case, it was not tested in this file.
We can use an index to control whether or not the $sort will spill, and we change the size of the
partition to control whether or not the $_internalSetWindowsFields will spill.

  
### Keywords
setWindowFields, percentile, spill 


## PipelineUpdate
### Owner
@mongodb/query
### Description
This workload runs both classic and pipeline-based updates. It records and monitors the metrics of
the two update types on documents of different sizes and shapes.



## ProjectParse
### Owner
@mongodb/query
### Description
Test $project parsing performance for small and large number of fields. We previously (before August 2023)
had a quadratic $project parsing algorithm which would take hours for the largest stages, due to a linear
search through all currently parsed fields rather than a map lookup. It should now take milliseconds to parse.
This issue was fixed by SERVER-78580, and discovered by SERVER-62509.

  
### Keywords
Project, Parsing, Aggregation 


## QueryStats
### Owner
@mongodb/query
### Description
This workload tests $queryStats performance. Specifically, it tests:
  - The general performance metrics of running $queryStats (latency, throughput, etc.)
  - The impact of running $queryStats to collect and report metrics while there are active queries
    trying to record their own metrics. We run a couple different experiments with different sized
    "payloads" (size of the find command shape).

Run `./run-genny evaluate src/workloads/query/QueryStats.yml` to see the output of
the preprocessor. See the [Genny docs](https://github.com/mongodb/genny/blob/master/docs/using.md#preprocessor)
for more information on using `evaluate`.



## QueryStatsQueryShapes
### Owner
@mongodb/query
### Description
This workload runs queries concurrently with both the same and different query shape.
This was designed to stress queryStats collection, but the workload doesn't actually enable queryStats.
It can be run with and without queryStats for comparison.



## RepeatedPathTraversal
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries over a set of paths which
share a common prefix. Crucially, these queries never match a document in the collection.



## RepeatedPathTraversalMedium
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries over a set of paths which
share a common prefix. Crucially, these queries never match a document in the collection.



## RepeatedPathTraversalSmall
### Owner
@mongodb/query
### Description
This workload stresses the query execution engine by running queries over a set of paths which
share a common prefix. Crucially, these queries never match a document in the collection.



## SSBColumnStoreIndex
### Owner
@mongodb/query-execution
### Description
This workload compares the query performance of SSB (star schema benchmark) queries with columnar indexes to performances without them.

The workload consists of the following phases and actors:
  1. Run SSB queries with columnstore indexes.
  2. Drop columnstore indexes.
  3. Run SSB queries without columnstore indexes.

The DSI test_control will drop caches inbetween each test to measure cold cache performance.

  
### Keywords
columnstore, analytics, SSB 


## SetWindowFieldsUnbounded
### Owner
@mongodb/query
### Description
This test exercises the behavior of $setWindowFields with an [unbounded, unbounded] window.
This case is interesting because it requires two passes over the documents (within each
partition): one pass to compute the window function, and one pass to write the new field to
each document.  If any partition is too large, it has to spill.  Also, internally we $sort by the
partition key, so that can also spill, depending on the collection size and available indexes.

We can divide this up into 4 cases:
  1. Nothing spills.
  2. Only the $sort spills.
  3. Only the $_internalSetWindowFields spills.
  4. Both the $sort and the $_internalSetWindowFields spill.

- To control whether the $sort spills, we can use an index, or not.
- To control whether the $_internalSetWindowFields spills, we can change the cardinality of
  the partition key.



## ShardFilter
### Owner
@mongodb/query
### Description
This workload tests the performance of queries which need to perform shard filtering.



## SlidingWindows
### Owner
@mongodb/query
### Description
This test exercises the behavior of '$setWindowFields' with sliding windows. Both time and
position based windows are tested. In an attempt to reduce noise, the collection size has been
kept sufficiently small so that neither the partitioning nor the sorting, in the case of time
based windows, will spill to disk.



## SlidingWindowsMultiAccums
### Owner
@mongodb/query
### Description
This test exercises the behavior of '$setWindowFields' with sliding windows, for multiaccumulators (like
$topN, $firstN, etc) and $top/$bottom which are not present in v5.0



## SortByExpression
### Owner
@mongodb/query
### Description
This test exercises the scenario of sort by an expression with an $addFields preceding the $sort,
followed by a $project removing that field. This workload is designed to avoid repeated materialization
after every projection in SBE.



## TenMDocCollection_IntId
### Owner
@mongodb/query
### Description
This workload tests the performance of IDHACK on queries with int _id versus non-IDHACK queries
on regular int indices.



## TenMDocCollection_IntId_Agg
### Owner
@mongodb/query
### Description
This workload tests the performance of IDHACK on queries with int _id, using the aggregation
framework.



## TenMDocCollection_IntId_IdentityView
### Owner
@mongodb/query
### Description
This workload tests the performance of IDHACK on queries with int _id over a no-op identity view.



## TenMDocCollection_IntId_IdentityView_Agg
### Owner
@mongodb/query
### Description
This workload tests the performance of IDHACK on queries with int _id over a no-op identity view,
using the aggregation framework.



## TenMDocCollection_ObjectId
### Owner
@mongodb/query
### Description
This workload tests performance of IDHACK on queries with OID _id.



## TenMDocCollection_ObjectId_Sharded
### Owner
@mongodb/query
### Description
This workload tests performance of IDHACK on queries with OID _id on a sharded collection.



## TenMDocCollection_SubDocId
### Owner
@mongodb/query
### Description
This workload tests performance of IDHACK on queries with sub-document _id.



## TimeSeries2dsphere
### Owner
@mongodb/query
### Description
This test exercises the behavior of querying for data in a timeseries collection using a 2dsphere index.

The phases are:
0. Create collection
1. Insert events + create index
2. Quiesce
3. $geoWithin
4. Quiesce
5. $geoNear



## TimeSeriesGroupStagesOnComputedFields
### Owner
@mongodb/query
### Description
The queries in this workload exercise group stage(s) after other stages ($addFields, $match, $sort)
on computed date fields on a timeseries collection.

  
### Keywords
timeseries, aggregate, sbe 


## TimeSeriesLastpoint
### Owner
@mongodb/query
### Description
This test exercises the behavior of lastpoint-type queries on time-series collections. The
currently supported lastpoint aggregate pipelines that are tested here include:
  1. a $sort on a meta field (both ascending and descending) and time (descending) and $group with _id on the meta
     field and only $first accumulators.
  2. a $sort on a meta field (both ascending and descending) and time (ascending) and $group with _id on the meta
     field and only $last accumulators.
  3. any of the above pipelines with a preceding match predicate on a meta field.



## TimeSeriesSort
### Owner
@mongodb/query
### Description
This test exercises the behavior of $_internalBoundedSort.
Test Overview:
- A time-series collection is created, and 100k docs are inserted.
- We run a sort on the time field.
- A descending index is created on the time field, and a descending sort on time is run. The
  descending index is then dropped.
- Another sort on the time field is run, simulating the time to first result.
- We create an index on {meta: 1, time: 1} for the following tests. We run a match on meta, and
  sort on time, matching 5k documents. Then we run the same test but only match 10 documents. The
  meta time index is dropped.
- 500k more documents are inserted for spilling tests. A sort on time is run, where the bounded
  sort should not spill.



## TimeSeriesSortCompound
### Owner
@mongodb/query
### Description
This test exercises the behavior of the time series bounded sorter with a compound sort.

We insert 1000 independent series with 100 buckets in each series, and each bucket has 100
documents. The documents inserted have the same timestamps, with different meta values.



## TimeSeriesSortOverlappingBuckets
### Owner
@mongodb/query
### Description
This test exercises the behavior of the time series bounded sorter with many overlapping buckets,
forcing the sort to spill to disk. We insert 1000 independent series with 100 buckets in
each series, and each bucket has 100 documents. The documents inserted have the same timestamps,
with different meta values.



## TimeSeriesTelemetry
### Owner
@mongodb/query
### Description
This test exercises the behavior of complex customer reports on top of time-series collections containing
telemetry data from arbitrary machines.

  
### Keywords
timeseries, aggregate, group 


## TimeseriesBlockProcessing
### Owner
@mongodb/query-execution
### Description
This workload runs queries on time-series collections that can at least partially run with block
processing. At the moment, only queries with a $match prefix are eligible for block processing.
The dataset used for this workload has uniformly random data so bucket level filtering is
ineffective. This stresses turning buckets into blocks and running block based operations on them.

  
### Keywords
timeseries, aggregate 


## TimeseriesCount
### Owner
@mongodb/query
### Description
The queries in this workload exercise group stage that uses the $count accumulator and the $count 
aggregation stage. On FCV greater than or equal to 7.1 $group using $count is optimized to remove 
the $unpack stage.

  
### Keywords
timeseries, aggregate, group 


## TimeseriesEnum
### Owner
@mongodb/query
### Description
The queries in this workload exercise group stage that uses an enum like field for the grouping.

  
### Keywords
timeseries, aggregate, group 


## TimeseriesFixedBucketing
### Owner
@mongodb/query-integration
### Description
This test exercises the behavior of the time series optimization when the collection has 
fixed buckets. This workload uses tsbs data that is imported in the dsi configuration. See 
'configurations/test_control/test_control.fixed_bucketing.yml' for all the details.
The data is set up with the fixed bucketing parameters set to 3600 and the timeField is "time"
and the metaField is "tags". There are 20736000 documents in the collection.

  
### Keywords
timeseries, aggregate, group 


## TimeseriesStressUnpacking
### Owner
@mongodb/query-integration
### Description
This test stress tests bucket unpacking. Each document has many top-level fields. Each query
targets a measurement, and cannot use the index on time/meta for time-series collections. Each
query will target a different number of buckets, and a different number of fields to unpack. We
should see the runtime for each query increase based on the number of buckets and fields to unpack.
We also compare the runtime of queries that project all fields explicitly vs returning the original
document "as is" with no projections defined.

  
### Keywords
timeseries, aggregate 


## TimeseriesTsbsExpressionQuery
### Owner
@mongodb/query
### Description
This workload runs different math expressions on time-series optimizations with the tsbs dataset. 
During development, This workload runs queries from the tsbs_query benchmark in Genny. It allows us 
to cross-check the results of the TSBS benchmarks, collect additional measurements, profile individual 
queries, and extend some of the queries as needed. The data is preloaded by the dsi configuration. See
'configurations/test_control/test_control.tsbs_query_genny.yml' for all  the details. There are 20736000
documents in the collection. There are some differences between the queries in this workload and tsbs,
but we do not expect these differences to significantly affect runtime:
1. TSBS will generate a random start time between the start/end time of the workload for every query type.
We hard code our time values, since genny does not currently support date arithmetic, nor storing a fixed
value from randomized generation.
2. TSBS randomizes which measurement fields to group by, we do not randomize these fields. Since all the fields
are random integers between 0-100 we don't expect different fields to have significant runtime differences.

  
### Keywords
timeseries, aggregate, group, sort 


## TimeseriesTsbsOptimizations
### Owner
@mongodb/query-integration
### Description
This workload runs different time-series optimizations with the tsbs dataset. During development,
we targeted optimizations that are most relevant to pushdown time-series queries to SBE. Further
optimizations should be added to this workload. The data is preloaded by the dsi configuration.
See 'configurations/test_control/test_control.tsbs_query_genny.yml' for all  the details.

Below are helpful statistics about the collection:
1. There are 20736000 documents and 161448 buckets in the collection.
2. "tags" is the metaField and "time" is the timeField.
3. "tags.hostname" has 100 values that are uniformly distributed.
4. The 50th percentile of the number of documents per bucket is 93.
5. The 80th percentile of the number of document per bucket is 274.96.
6. 40435 buckets are version 1 and 121013 buckets are version 2.
7. Predicates on a single measurement value, such as {usage_nice: 5} will hit around 18% of buckets.
8. All measurement fields vary from [0, 100]. Not all measurement fields have similar distributions.
You need to verify how many buckets are hit for each measurement field predicate written.
9. Predicates of bucket at a time window of 1 hour is expected to hit 0.2% of buckets.
10. The earliest date is  "2016-01-01T00:00:00Z" and the latest date is "2016-01-24T23:59:50Z".

  
### Keywords
timeseries, aggregate, group, sort 


## TimeseriesTsbsQuery
### Owner
@mongodb/query-integration
### Description
This workload runs different time-series optimizations with the tsbs dataset. During development,
This workload runs queries from the tsbs_query benchmark in Genny. It allows us to cross-check
the results of the TSBS benchmarks, collect additional measurements, profile individual queries,
and extend some of the queries as needed. The data is preloaded by the dsi configuration. See
'configurations/test_control/test_control.tsbs_query_genny.yml' for all  the details. There are 20736000
documents in the collection. There are some differences between the queries in this workload and tsbs,
but we do not expect these differences to significantly affect runtime:
1. TSBS will generate a random start time between the start/end time of the workload for every query type.
We hard code our time values, since genny does not currently support date arithmetic, nor storing a fixed
value from randomized generation.
2. TSBS randomizes which measurement fields to group by, we do not randomize these fields. Since all the fields
are random integers between 0-100 we don't expect different fields to have significant runtime differences.

  
### Keywords
timeseries, aggregate, group, sort 


## UnionWith
### Owner
@mongodb/query
### Description
This workload exercises '$unionWith' with two or more collections in multiple scenarios, including
collections of high overlap, disjoint collections, multiple sequential unions, nested unions, and
unions with complex subpipelines. These tests are run on standalones, replica sets, and sharded
environments.



## UnwindGroup
### Owner
@mongodb/query
### Description
This test exercises a simple [$unwind, $group] aggregation pipeline to enable performance
comparison between the Classic and SBE execution engines when pushing $unwind down to SBE is
enabled (whole pipeline runs in SBE) versus disabled (whole pipeline runs in Classic engine).

  
### Keywords
unwind, group, aggregate 


## UpdateLargeDocuments
### Owner
@mongodb/query
### Description
This workload measures the performance of updating large documents in a replica set. First, it
inserts large documents, each containing 100 nested objects with 1K fields inside of them. Then it
runs a series of simple updates. Each update is setting only one field and has write concern
"majority". The output of this workload is ops/sec, indicating how many of these simple updates
were finished in a period of time.



## VariadicAggregateExpressions
### Owner
@mongodb/query-execution
### Description
This workload measures the performance of aggregation expressions that
can take in a variable number of expressions as arguments.

While measuring performances, this workload collects numbers
for either the SBE or the classic engine aggregation expression
implementations, depending on environments that it runs on.

Numbers on the 'standalone-all-feature-flags' environment are for
the SBE variadic aggregation expressions and numbers on the 'standalone'
environment for the classic variadic aggregation expressions.

  
### Keywords
aggregate, sbe 


## WindowWithComplexPartitionExpression
### Owner
@mongodb/query
### Description
This test exercises the behavior of '$setWindowFields' with sliding windows with complex
partitionBy expression.



## WindowWithNestedFieldProjection
### Owner
@mongodb/query
### Description
This test exercises the performance of '$setWindowFields' projecting a nested field.



## linearFill
### Owner
@mongodb/query
### Description
This workload tests the performance of $linearFill. If there is a nullish value at the evaluated expression,
this window function uses the difference on the sortBy field to calculate the percentage of the
missing value range that should be covered by this document, and fills that document proportionally.

The benchmark operations test integer and double data types with single and multiple outputs.
There is a test without partitions, with a single partition, and with multiple partitions
for both single and multiple outputs. Partitions require $linearFill to be done individually
on each partition, which requires additional tracking and may make queries slower.

To learn more about partitions, please check out the docs here:
https://docs.mongodb.com/manual/reference/operator/aggregation/setWindowFields/



## locf
### Owner
@mongodb/query
### Description
This workload tests the performance of $locf. $locf stands for "Last Observed Carry Forward".
If there is a nullish value at the evaluated expression, it is replaced in the output field by the
last non-nullish value. Otherwise the value is simply copied over. The benchmark operations test numeric,
string, date and randomized data types. There is a test without partitions, with a
single partition, and with multiple partitions for each data type. Partitions require carry forward
to be done individually on each partition, which requires additional tracking and may make queries
slower.

To learn more about partitions, please check out the docs here:
https://docs.mongodb.com/manual/reference/operator/aggregation/setWindowFields/



## BlockingSort
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run
a query that makes all of them eligible, so we get as many competing plans as possible. We also
add a sort stage on an unindexed field, ensuring that every plan is a blocking plan. Because all
plans are blocking and return as many documents as possible, multiplanning will hit "max works"
instead of EOF of numToReturn. This maximizes the overhead of multiplanning on both classic and SBE.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## ClusteredCollection
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
 query that makes all of them eligible, so we get as many competing plans as possible. Here, we do this on a
 clustered collection that has very large strings as _id.

 We expect classic to have better latency and throughput than SBE on this workload,
 and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
 as well as classic.



## CompoundIndexes
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many compound indexes as possible, each
comprising of tenantId and another field. We then run a query with an equality predicate on tenantId and 
other predicates a small number of fields, so we get all the plans that use relevant indexes.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## ManyIndexSeeks
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning with many index seeks. We create as many compound
indexes as possible, each comprising of two fields. One of the indices (x1, ...) is designed to be
the most effective index, while the rest of them (..., x1) are very ineffective. We then run a query
having predicates on all the fields, while only the predicate on field x1 has selective range. This
leads to many index seeks on the less effective indices (..., x1). Because every time we hit a non-
matching field we seek again, and the scan ends when we reach a non-matching x1.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## MultiPlanningReadsALotOfData
### Owner
@mongodb/query-execution
### Description
Create collection with multiple indexes and run queries for which multi planner needs to read a lot
of data in order to pick the best plan.

First CrudActor QueryWithMultiplanning will use queries specifically design to trick tie
breaking heuristics, so that if the planner doesn't reach enough data, it will pick the wrong
index.

Second CrudActor QueryWithMultiplanningAndTieBreakingHeuristics will use queries for which tie
breaking heuristics are guessing the correct index.
  In both cases, heuristics lead us to pick the (flag_a, flag_b) index.  In the first case, this is a bad choice:
  the two boolean predicates select 25% of documents while the int_a predicate is much narrower.
  In the second case, we still choose the (flag_a, flag_b) index, but this time it is the correct choice,
  because the int_a predicate matches >50% of documents.

  
### Keywords
indexes 


## MultikeyIndexes
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning with multikey indexes. We create many indexes and
run a query that makes all of them eligible, so we get as many competing plans as possible. Because 
an IXSCAN of a multikey index has to deduplicate RIDs, a lot of space will be used. The classic 
multi-planner will behave more optimally than the SBE multiplanner because it will cut off execution
when the one good plan reaches the end.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## MultiplannerWithGroup
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
query that makes all of them eligible, so we get as many competing plans as possible. We add a
group stage, which is blocking. The SBE multiplanner will multiplan group as it is a part of the
canonical query, but the classic multiplanner will not plan. This means the SBE multiplanner will
have the overhead of trial running blocking plans when compared to the classic multiplanner.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## NoGoodPlan
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create 64 indexes and run a query that
makes all of them eligible, so we get as many competing plans as possible. The only selective
field is unindexed, however, meaning no index will be effective in planning. By ensuring all plans
are relatively equally bad, we are likely to hit the works limit sooner than the 101 results
limit.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## NoResults
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
query that makes all of them eligible, so we get as many competing plans as possible. All predicates
are very selective (match 0% of the documents). With zero results, we do no hit the EOF optimization
and all competing plans hit the works limit instead of document limit.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## NoSuchField
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
query that makes all of them eligible, so we get as many competing plans as possible. Here, we add
an additional predicate: {no_such_field: "none"} to guarantee that we hit getTrialPeriodMaxWorks().

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## NonBlockingVsBlocking
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. If the selectivity value is small enough (less
than 0.5), the optimal plan is to employ a blocking plan by scanning a segment of empty data and
conducting a blocking-sort operation, whereas the other plans' index provides the right sort
order, but requires a full scan, and every document is rejected after the FETCH stage. Because the
SBE multiplanner can't round-robin, it has a heuristic "try nonblocking plans first".  This
scenario is a worst case for that heuristic, because we'll try the best plan last. Otherwise, an
IXSCAN and FETCH non-blocking plan will be used.

We expect classic to have better latency and throughput than SBE on this workload, and we expect
the combination of classic planner + SBE execution (PM-3591) to perform about as well as classic.



## Simple
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
query that makes all of them eligible, so we get as many competing plans as possible.

The original goal of this test was to demonstrate weaknesses of the SBE multiplanner when compared to
the classic multiplanner. Mainly, the SBE multiplanner can't round-robin between plans, which means it
has to run the list of plans sequentially, which means we can't short-circuit when the shortest-running
plan finishes.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## Subplanning
### Owner
@mongodb/query
### Description
The goal of this test is to exercise subplanning. We create as many indexes as possible, and run a
query that makes all of them eligible, so we get as many competing plans as possible.

The workload uses an $or query with 8 clauses each containing 8 predicates. Each branch have
only one selective predicate.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## UseClusteredIndex
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We create as many indexes as possible, and run a
 query that makes all of them eligible, so we get as many competing plans as possible. Here, we do this on a
 clustered collection and add a selective predicate on _id, so that the clustered index is a viable candidate plan.

 We expect classic to have better latency and throughput than SBE on this workload,
 and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
 as well as classic.



## VariedSelectivity
### Owner
@mongodb/query
### Description
The goal of this test is to exercise multiplanning. We run the same query 7 times, each one with a
different selectivity value that we are comparing against x1, calcuated based on the number of
documents we want the query to match. This will help us measure the overhead of throwing out the
result set gathered during multi-planning when the result set exceeds 101 documents.  Unlike many
of the other multiplanner/ workloads, we only test with 2 indexes here, because 2 indexes is a
worst case for throwing away results. Having more indexes increases planning time, but not query
execution time, so having more indexes makes the *relative* cost of throwing away results smaller.

We expect classic to have better latency and throughput than SBE on this workload,
and we expect the combination of classic planner + SBE execution (PM-3591) to perform about
as well as classic.



## dbcheck_40GB
### Owner
mongodb/server-replication"
### Description
Measures the performance of running dbcheck's modes and its effect on crud operations.

  
### Keywords
dbcheck, collections, indexes, crud 


## 1_0_5GB
### Owner
mongodb/server-replication"
### Description
Loads the data for the light phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to not having any ops to be applied
during startup recovery:

Sample logs:
```
+-------------------------------------------------------------------------------------------+
| {                                                                                         |
|     s: "I",                                                                               |
|     c: "REPL",                                                                            |
|     id: 21549,                                                                            |
|     ctx: "initandlisten",                                                                 |
|     msg: "No oplog entries to apply for recovery. Start point is at the top of the oplog" |
|   }                                                                                       |
+-------------------------------------------------------------------------------------------+
```

  
### Keywords
startup, collections, indexes, defaultWC 


## 1_1_5GB_crud
### Owner
mongodb/server-replication"
### Description
Adds CRUD operations to be replayed during startup recovery for the light phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to have alot of CRUD ops to be
applied during startup recovery:

Sample logs:
```
+--------------------------------------------------------+
|  {                                                     |
|       s: "I",                                          |
|       c: "REPL",                                       |
|       id: 21536,                                       |
|       ctx: "initandlisten",                            |
|       msg: "Completed oplog application for recovery", |
|       attr: {                                          |
|           numOpsApplied: 50207,                        |
|           numBatches: 142,                             |
|           applyThroughOpTime: {                        |
|               ts: {                                    |
|                   $timestamp: {                        |
|                       t: 1690277528,                   |
|                       i: 1                             |
|                   }                                    |
|               },                                       |
|               t: 6                                     |
|           }                                            |
|       }                                                |
|   }                                                    |
+--------------------------------------------------------+
```

  
### Keywords
startup, stopCheckpointing, updates 


## 1_2_5GB_ddl
### Owner
mongodb/server-replication"
### Description
Adds DDL operations to be replayed during startup recovery for the light phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to have alot of DDL ops to be
applied during startup recovery:

Sample logs:
```
+------------------------------------------------------------------------------------+
|   1- Alot of "Dropping unknown ident".                                             |
|   2- Alot of createCollection ops:                                                 |
|   {                                                                                |
|       s: "I",                                                                      |
|       c: "REPL",                                                                   |
|       id: 7360109,                                                                 |
|       ctx: "initandlisten",                                                        |
|       msg: "Processing DDL command oplog entry in OplogBatcher",                   |
|       attr: {                                                                      |
|           oplogEntry: {                                                            |
|               oplogEntry: {                                                        |
|                   op: "c",                                                         |
|                   ns: "startup_5GB_ddl.$cmd",                                      |
|                   o: {                                                             |
|                       create: "Collection2",                                       |
|                       idIndex: {                                                   |
|                           v: 2,                                                    |
|                           key: {                                                   |
|                               _id: 1                                               |
|                           },                                                       |
|                           name: "_id_"                                             |
|                           ...                                                      |
|   }                                                                                |
|   3- Alot of IndexBuilds:                                                          |
|   {                                                                                |
|     s: "I",                                                                        |
|     c: "STORAGE",                                                                  |
|     id: 5039100,                                                                   |
|     ctx: "IndexBuildsCoordinatorMongod-2",                                         |
|     msg: "Index build: in replication recovery. Not waiting for last optime before |
|     interceptors to be majority committed",                                        |
|   }                                                                                |
+------------------------------------------------------------------------------------+
```

  
### Keywords
startup, stopCheckpointing, collections, indexes 


## 1_3_5GB_index
### Owner
mongodb/server-replication"
### Description
Initiates an index build to be continued during startup recovery for the light load phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to not have any ops to be applied
during startup recovery but we should see continuation of the index builds.

Sample logs:
```
+-----------------------------------------------------------------+
|   1- Alot of "Dropping unknown ident".                          |
|   2- Continuation of the index build:                           |
|   {                                                             |
|     s: "I",                                                     |
|     c: "STORAGE",                                               |
|     id: 22253,                                                  |
|     ctx: "initandlisten",                                       |
|     msg: "Found index from unfinished build",                   |
|     attr: {                                                     |
|         namespace: "startup_5GB.Collection0",                   |
|         index: "index_int5_int6",                               |
|         buildUUID: {                                            |
|             uuid: {                                             |
|                 $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9"   |
|             }                                                   |
|         }                                                       |
|     }                                                           |
|   }                                                             |
|   {                                                             |
|       t: {                                                      |
|           $date: "2023-07-25T10:08:47.569+00:00"                |
|       },                                                        |
|       s: "I",                                                   |
|       c: "STORAGE",                                             |
|       id: 4841700,                                              |
|       ctx: "initandlisten",                                     |
|       msg: "Index build: resuming",                             |
|       attr: {                                                   |
|           buildUUID: {                                          |
|               uuid: {                                           |
|                   $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9" |
|               }                                                 |
|           },                                                    |
|           namespace: "startup_5GB.Collection0",                 |
|           ...                                                   |
|   }                                                             |
|   {                                                             |
|       s: "I",                                                   |
|       c: "INDEX",                                               |
|       id: 20346,                                                |
|       ctx: "initandlisten",                                     |
|       msg: "Index build: initialized",                          |
|       attr: {                                                   |
|           buildUUID: {                                          |
|               uuid: {                                           |
|                   $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9" |
|               }                                                 |
|           },                                                    |
|           namespace: "startup_5GB.Collection0",                 |
|       }                                                         |
|   }                                                             |
+-----------------------------------------------------------------+
```

  
### Keywords
startup, hangIndexBuild, collections, indexes, stepdown, stepup 


## 2_0_50GB
### Owner
mongodb/server-replication"
### Description
Loads the data for the heavy phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to not having any ops to be applied
during startup recovery:

Sample logs:
```
+-------------------------------------------------------------------------------------------+
| {                                                                                         |
|     s: "I",                                                                               |
|     c: "REPL",                                                                            |
|     id: 21549,                                                                            |
|     ctx: "initandlisten",                                                                 |
|     msg: "No oplog entries to apply for recovery. Start point is at the top of the oplog" |
|   }                                                                                       |
+-------------------------------------------------------------------------------------------+
```

  
### Keywords
startup, collections, indexes 


## 2_1_50GB_crud
### Owner
mongodb/server-replication"
### Description
Adds CRUD operations to be replayed during startup recovery for the heavy phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to have alot of CRUD ops to be
applied during startup recovery:

Sample logs:
```
+--------------------------------------------------------+
|  {                                                     |
|       s: "I",                                          |
|       c: "REPL",                                       |
|       id: 21536,                                       |
|       ctx: "initandlisten",                            |
|       msg: "Completed oplog application for recovery", |
|       attr: {                                          |
|           numOpsApplied: 50207282,                     |
|           numBatches: 10042,                           |
|           applyThroughOpTime: {                        |
|               ts: {                                    |
|                   $timestamp: {                        |
|                       t: 1690277528,                   |
|                       i: 1                             |
|                   }                                    |
|               },                                       |
|               t: 6                                     |
|           }                                            |
|       }                                                |
|   }                                                    |
+--------------------------------------------------------+
```

  
### Keywords
startup, stopCheckpointing, updates 


## 2_2_50GB_ddl
### Owner
mongodb/server-replication"
### Description
Adds DDL operations to be replayed during startup recovery for the heavy phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to have alot of DDL ops to be
applied during startup recovery:

Sample logs:
```
+------------------------------------------------------------------------------------+
|   1- Alot of "Dropping unknown ident".                                             |
|   2- Alot of createCollection ops:                                                 |
|   {                                                                                |
|       s: "I",                                                                      |
|       c: "REPL",                                                                   |
|       id: 7360109,                                                                 |
|       ctx: "initandlisten",                                                        |
|       msg: "Processing DDL command oplog entry in OplogBatcher",                   |
|       attr: {                                                                      |
|           oplogEntry: {                                                            |
|               oplogEntry: {                                                        |
|                   op: "c",                                                         |
|                   ns: "startup_50GB_ddl.$cmd",                                     |
|                   o: {                                                             |
|                       create: "Collection9500",                                    |
|                       idIndex: {                                                   |
|                           v: 2,                                                    |
|                           key: {                                                   |
|                               _id: 1                                               |
|                           },                                                       |
|                           name: "_id_"                                             |
|                           ...                                                      |
|   }                                                                                |
|   3- Alot of IndexBuilds:                                                          |
|   {                                                                                |
|     s: "I",                                                                        |
|     c: "STORAGE",                                                                  |
|     id: 5039100,                                                                   |
|     ctx: "IndexBuildsCoordinatorMongod-2",                                         |
|     msg: "Index build: in replication recovery. Not waiting for last optime before |
|     interceptors to be majority committed",                                        |
|   }                                                                                |
+------------------------------------------------------------------------------------+
```

  
### Keywords
startup, stopCheckpointing, collections, indexes 


## 2_3_50GB_index
### Owner
mongodb/server-replication"
### Description
Initiates an index build to be continued during startup recovery for the heavy load phase.
To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

Expected behavior:
--------------
We expect after restarting mongod after running this workload to not have any ops to be applied
during startup recovery but we should see continuation of the index builds during initialization.

Sample logs:
```
+-----------------------------------------------------------------+
|   1- Alot of "Dropping unknown ident".                          |
|   2- Continuation of the index build:                           |
|   {                                                             |
|     s: "I",                                                     |
|     c: "STORAGE",                                               |
|     id: 22253,                                                  |
|     ctx: "initandlisten",                                       |
|     msg: "Found index from unfinished build",                   |
|     attr: {                                                     |
|         namespace: "startup_50GB.Collection0",                  |
|         index: "index_int5_int6",                               |
|         buildUUID: {                                            |
|             uuid: {                                             |
|                 $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9"   |
|             }                                                   |
|         }                                                       |
|     }                                                           |
|   }                                                             |
|   {                                                             |
|       t: {                                                      |
|           $date: "2023-07-25T10:08:47.569+00:00"                |
|       },                                                        |
|       s: "I",                                                   |
|       c: "STORAGE",                                             |
|       id: 4841700,                                              |
|       ctx: "initandlisten",                                     |
|       msg: "Index build: resuming",                             |
|       attr: {                                                   |
|           buildUUID: {                                          |
|               uuid: {                                           |
|                   $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9" |
|               }                                                 |
|           },                                                    |
|           namespace: "startup_50GB.Collection0",                |
|           ...                                                   |
|   }                                                             |
|   {                                                             |
|       s: "I",                                                   |
|       c: "INDEX",                                               |
|       id: 20346,                                                |
|       ctx: "initandlisten",                                     |
|       msg: "Index build: initialized",                          |
|       attr: {                                                   |
|           buildUUID: {                                          |
|               uuid: {                                           |
|                   $uuid: "8d713ff8-88e8-4198-a15d-7d590acd9cb9" |
|               }                                                 |
|           },                                                    |
|           namespace: "startup_50GB.Collection0",                |
|       }                                                         |
|   }                                                             |
+-----------------------------------------------------------------+
```

  
### Keywords
startup, hangIndexBuild, collections, indexes, stepdown, stepup 


## 3_0_Reads
### Owner
mongodb/server-replication"
### Description
Issues dummy reads against both databases used in the light and the heavy phases.
 To know more about the test phases please refer to 'src/workloads/replication/startup/README.md'.

  
### Keywords
startup, reads 


## BigUpdate
### Owner
@mongodb/product-perf
### Description
This workload is developed as a general stress test of the server.
It loads data into a large number of collections, with 9 indexes on each collection. After loading
the data, a fraction of the collections are queried, and a smaller fraction of collections are
updated.

  
### Keywords
stress, collections, indexes, update, find, coldData 


## BigUpdate10k
### Owner
@mongodb/product-perf
### Description
This workload is developed as a general stress test of the server. It loads data into a large
number of collections, with 9 indexes on each collection. After loading the data, a fraction of
the collections are queried, and a smaller fraction of collections are updated. This is the larger
version of the test, using 10k collections and 10k documents per collection.

  
### Keywords
stress, collections, indexes, update, find, coldData 


## BulkLoading
### Owner
@mongodb/product-perf
### Description
This workload tests the performance of the bulkWrite API, and compares it to normal writes.
It is modeled as a collection of sensor data. It would be common for this use case to do batching using, e.g., AWS SQS.
The thread levels for the many connections scenario are tuned for maximum throughput.

It tests the following scenarios:
- BulkWrite API, ~30GB data in the db, single connection, no index
- BulkWrite API, ~30GB data in the db, single connection, with index

- BulkWrite API, ~30GB data in the db, many connections, no index
- BulkWrite API, ~30GB data in the db, many connections, with index

- normal writes, ~30GB data in the db, many connections, no index
- normal writes, ~30GB data in the db, many connections, with index

- BulkWrite Upserts, ~30GB data in the db, many connections, with index

  
### Keywords
bulk insert, bulk write, indexes, upsert, stress 


## CollScan
### Owner
@mongodb/product-perf
### Description
This workload loads 10M rows into a collection, then executes collection scans in a single thread.

  
### Keywords
collection scan 


## ContentionTTLDeletions
### Owner
@mongodb/server-execution
### Description
This workload tests the impact of background TTL deletions in a heavily modified collection with
concurrent crud operations on a second collection to simulate extreme ticket contention.

  
### Keywords
ttl, stress, indexes, insertMany, CrudActor 


## CreateDropView
### Owner
@mongodb/product-perf
### Description
This workload loads a collection with documents and then repeatedly
create and drops a view on that collection.

  
### Keywords
view, create, drop 


## CursorStormMongos
### Owner
@mongodb/query
### Description
Used to test performance of the router under memory pressure caused by accumulating
many heavy cursors. The workload is expected to fail due to host(s) being unreachable as a
result of mongos running out of memory.

To achieve this, many threads are spawned to run an unfiltered find on a collection.
The number and size of documents in that collection are tuned such, that the mongos is able
to exhaust cursors on shards when pre-filling its buffers [<16MB per shard]. As a result, 
memory pressure on the shards remains low, while it's kept large on the mongos.

  
### Keywords
scale, memory stress, cursor storm, mongos, fail, oom, out of memory 


## InCacheSnapshotReads
### Owner
Replication
### Description
TODO: TIG-3321



## InsertBigDocs
### Owner
@mongodb/replication
### Description
TODO: TIG-3321



## InsertRemove
### Owner
@mongodb/product-perf
### Description
Demonstrate the InsertRemove actor. The InsertRemove actor is a simple actor that inserts and then
removes the same document from a collection in a loop. Each instance of the actor uses a different
document, indexed by an integer _id field. The actor records the latency of each insert and each
remove.

  
### Keywords
docs, actorInsertRemove, insert, delete 


## LargeIndexedIns
### Owner
@mongodb/product-perf
### Description
This workload sends bursts of finds that use large $ins. The workload
causes high CPU load as the server becomes bottlenecked on tcmalloc
spinlocks during the find operations.

Improvements or regressions in this aspect of the allocator should
be measurable by the average CPU usage during this test as well as
the latency of the find operations.

In this workload, large arrays of random strings are generated to use
for the $in queries.  To avoid a CPU bottleneck on the workload client,
it uses a ^Once generator to generate the arrays once during initialization.



## LargeScaleLongLived
### Owner
Storage Engines
### Description
This workload consists of two phases intended to test the basic long lived reader writer actors
created for the large scale workload automation project. It creates a database with 10K
collections and 10 indexes per collection. It reads at 15K op/s and writes at 5K op/s.



## LargeScaleModel
### Owner
Storage Engines
### Description
The model workload for the large scale workload automation project. It consists of a 150GB cold
database which is scanned by the snapshot scanner and a number of hot db collections and rolling
db collections. It is expected to be written at a rate of 10K writes per second and read at 1K
reads per second.



## LargeScaleParallel
### Owner
STM
### Description
See LargeScaleSerial.yml for a general overview of what this workload does. The main
difference here is that the update is parallel with the long-running query and
multi-collection scan. This adds some concurrent write load.

  
### Keywords
collections, oltp, update, query, scale 


## LargeScaleSerial
### Owner
STM
### Description
This config simulates a "typical" large-scale OLTP workload running in parallel with a
slower analytical operation. This is a use-case intended to improve with the addition of
durable history.

The first phase loads data while performing updates, lasts for one hour, and expects an
overall update throughput of ten updates per millisecond.

The second phase is a no-op.

The third phase lasts for another hour, and runs three parallel query operations: a
warmup that queries 100 collections, and two larger ones that query all 10K
collections. These aren't performance-sensitive, they just serve to keep various "older"
records around (hence the rate-limiting). The queries run in parallel with a full
collection scan, which represents a conventional performance-sensitive OLTP workload
that shouldn't be too affected by the long-running queries.

  
### Keywords
collections, oltp, query, scale 


## LoadTest
### Owner
@mongodb/product-perf
### Description
Based on LongLivedTransactions Insert workload. Using to experiment with bulk load for PERF-2330



## MajorityReads10KThreads
### Owner
@mongodb/server-execution
### Description
This workload simulates a case of extreme overload with a majority of reads happening.



## MajorityWrites10KThreads
### Owner
@mongodb/server-execution
### Description
This workload simulates a case of extreme overload with a majority of writes happening.



## ManyUpdate
### Owner
@mongodb/server-replication
### Description
This workload loads a large number of small documents into a single collection, and then does a
small multi-update which touches all of them.  It is intended to measure the impact of the load of
replication of many oplog entries on the primary.  Number of documents should be significantly
greater than the maximum replication batch size of 5K (50K is a good minimum).
We will run this against standalone nodes and single-node replica sets as well as 3-node replica
sets to determine if any performance changes are due to replication overhead changes (if only
3-node replica sets are affected) or some other reason.

  
### Keywords
RunCommand, Loader, CrudActor, updateMany, update, replication, oplogSourceOverhead 


## MassDeleteRegression
### Owner
@mongodb/product-perf
### Description
This workload is a repro of SERVER-48522, a regression in the document
remove rate. It loads, documents, removes them and then queries for
the removed documents. A bisect, build, test cycle was run as part of
PERF-2075 to find the cause of SERVER-48522



## Mixed10KThreads
### Owner
@mongodb/server-execution
### Description
This workload consists of a situation where the server is being contacted by 10k different
clients to simulate an extreme case of overload in the server. Both reads and writes happen
at the same time in balanced fashion.

The metrics to monitor are:
  * ErrorsTotal / ErrorRate: The total number of errors and rate of errors encountered by the workload. Networking
    errors are not unexpected in general and they should be recoverable. This work load strives to provide a test to
    allow us to measure the scale of networking errors in a stressful test and prove if the networking becomes more
    stable (with lower total errors and a lower error rate).
  * The Operation latency (more specifically the Latency90thPercentile to Latency99thPercentile metrics)
  * The Operation Throughput
  * "ss connections active": the number of connections.

  
### Keywords
scale, insertMany, find 


## MixedWorkloadsGenny
### Owner
@mongodb/product-perf
### Description
This workload is a port of the mixed_workloads in the workloads
repo. https://github.com/10gen/workloads/blob/master/workloads/mix.js. It runs 4 sets of
operations, each with dedicated actors/threads. The 4 operations are insertOne, findOne,
updateOne, and deleteOne. Since each type of operation runs in a dedicated thread it enables
interesting behavior, such as reads getting faster because of a write regression, or reads being
starved by writes. The origin of the test was as a reproduction for BF-2385 in which reads were
starved out by writes.

  
### Keywords
scale, insertOne, insert, findOne, find, updateOne, update, deleteOne, delete 


## MixedWorkloadsGennyRateLimited
### Owner
@mongodb/product-perf
### Description
This workload is a Rate Limited version of  MixedWorkloadsGenny, which is a port of the mixed_workloads in the workloads repo.
https://github.com/10gen/workloads/blob/master/workloads/mix.js. It runs 4 sets of
operations with a single dedicated actor/thread. The 4 operations are insertOne, findOne,
updateOne, and deleteOne. Previously, each type of operation ran in a dedicated thread which resulted in
high CPU utilization of around 90%. The Update operation was found to cause the highest CPU usage.
Rate Limits were added to maintain CPU utlization at 30-40%, along with using a higher thread level and decreasing phase number count from 4 to 1.

  
### Keywords
scale, insertOne, insert, findOne, find, updateOne, update, deleteOne, delete, rateLimited, globalRate 


## MixedWorkloadsGennyStress
### Owner
@mongodb/product-perf
### Description
This workload is a more stressful version of MixedWorkloadsGenny.yml, which itself is a port of
the mixed_workloads in the workloads,
repo. https://github.com/10gen/workloads/blob/master/workloads/mix.js. It runs 4 sets of
operations, each with dedicated actors/threads. The 4 operations are insertOne, findOne,
updateOne, and deleteOne. Since each type of operation runs in a dedicated thread it enables
interesting behavior, such as reads getting faster because of a write regression, or reads being
starved by writes. The origin of the test was as a reproduction for BF-2385 in which reads were
starved out by writes.

This more stressful version of the test only runs one test phase, using 1024 threads per operation
for 45 minutes.

  
### Keywords
scale, insertOne, insert, findOne, find, updateOne, update, deleteOne, delete 


## MixedWorkloadsGennyStressWithScans
### Owner
@mongodb/product-perf
### Description
This workload is a more stressful version of MixedWorkloadsGenny.yml, which itself is a port of
the mixed_workloads in the workloads repo.
https://github.com/10gen/workloads/blob/master/workloads/mix.js. In particular, this workload
extends MixedWorkloadsGennyStress.yml by adding aggregations that perform index scans. It runs 5
sets of operations, each with dedicated actors/threads. The 5 operations are insertOne, findOne,
updateOne, deleteOne, and aggregate. Since each type of operation runs in a dedicated thread it
enables interesting behavior, such as reads getting faster because of a write regression, or reads
being starved by writes. The origin of the test was as a reproduction for BF-2385 in which reads
were starved out by writes.

This more stressful version of the test only runs one test phase, using 1024 threads per operation
for 45 minutes.

  
### Keywords
scale, insertOne, insert, findOne, find, updateOne, update, deleteOne, delete, aggregate, scan 


## MixedWrites
### Owner
@10gen/dev-prod-tips
### Description
Does w:2 writes for a Phase followed
by w:3 writes for a second Phase.

Requires at least 3-node replset.



## NegativeScalingLoadStress
### Owner
@mongodb/product-perf
### Description
This workload is intended to be used to stress a system that may show negative scaling behaviour.
The workload will insert 100k documents and then run 100k finds across 256 threads. This workload
has been shown to identify negative scaling on dual socket instances and was originally created to
mimic the behvaiour reported in HELP-44821. Key metrics to observe here are operations per second,
cpu kernel usage and cpu user usage. If we see high CPU kernel usage plus low ops/s, we may be observing
negative scaling. This workload is not scheduled to run at this time and is intended for adhoc use.

  
### Keywords
scale, insertOne, insert, findOne, find 


## OutOfCacheScanner
### Owner
Storage Engines
### Description
A workload which creates 2 databases a "cold" database which is scanned periodically and a "hot"
database which is read continuously throughout the workload. The intention is that the hot
database is large enough that it fills WiredTiger's cache and the WiredTiger cache takes up most
of the system memory so the operating system cache is also not big enough for the cold database.
Thus the cold data must be read directly from disk. The read latency by the RandomSampler is
recorded at all times.



## OutOfCacheSnapshotReads
### Owner
Replication
### Description
TODO: TIG-3321



## ReadMemoryStressUntilFailure
### Owner
Service Architecture
### Description
Used to test performance of the server under heavy memory pressure caused by read
operations. This workload is expected to fail due to host(s) being unreachable as a
result of mongod(s) running out of memory.

To achieve this, documents are inserted in the form of {a: <id>, b: <random number>,
c: <fill 520 KB>}. Many threads are then spawned to run aggregate sort on a number of
documents. The document size and the number of documents to sort by each thread are
tuned to be just under 100 MB, which is the memory limit for operations before
spilling to disk. The threads would therefore each cause a close-to-max amount of
memory to be used. Increasing the number of threads should cause the host(s) that
process the operations to fail due to out-of-memory errors.

  
### Keywords
scale, memory stress, aggregate, sort, insert, fail, oom, out of memory 


## ReplaceMillionDocsInSeparateTxns
### Owner
@mongodb/sharding
### Description
This workload is developed to test the amount of time it takes to remove and re-insert one
million documents, with a fixed transaction batch size of one hundred.

  
### Keywords
transactions, stress, time 


## ScanWithLongLived
### Owner
Storage Engines
### Description
This workload is designed to test the effectiveness of durable history as
described in PM-1986.



## TimeSeriesSortScale
### Owner
@mongodb/query
### Description
This test exercises the behavior of the time series bounded sorter as the number of documents
in the collection increases.  The collection has 10 million documents, and each document has a
random meta value ranging from 0 to 1000.



## UniqueIndexStress
### Owner
@mongodb/product-perf
### Description
This workload tests insert performance with unique indexes. Each pair of phases first, drops the
database, then creates 7 indexes, before inserting documents as fast as it can. The difference
between the different phases is how many of the indexes are unique. It first does 0 unique
secondary indexes, then 1, 2, up to 7.

  
### Keywords
insert, unique indexes 


## UpdateMillionDocsInTxn
### Owner
@mongodb/sharding
### Description
This workload is developed to test the amount of time it takes to update a million documents in
a single replica set transaction. At the moment, the average time taken raises above the default
limit, so until we add a way to manually increase a transaction's lifetime, we must raise the
lifetime of all transactions.

  
### Keywords
transactions, stress, time 


## UpdateSingleLargeDocumentWith10kThreads
### Owner
@mongodb/query
### Description
The workload inserts a single large document, and tests concurrently updating the same document
with many threads. This is intended to test the behavior of the server under heave memory pressure
caused by update operations with a high rate of write conflicts. The update operation is on an
integer field, the command itself is relatively small, so most of the memory pressure should come
from the query subsystem.

  
### Keywords
CrudActor, Loader, memory, scale, stress, updateOne, WriteConflict 


## GennyOverhead
### Owner
@10gen/dev-prod-tips
### Description

This workload measures the overhead of Genny itself. There are 2 different
configurations that run consecutively for 5 phases each. The first
configuration runs with 100 threads and the second configuration runs
with 1 thread.

The 5 phases of each configuration are as follows:

1. Loop for 10 seconds using the "Duration" keyword
2. Loop for 3M iterations using the "Repeat" keyword. Each iteration
   takes around 5µs when run in a single thread.
3. Loop for 10k iterations with a 1 per 1ms rate limit
4. Loop for 10k iterations while sleeping for 1ms before each iteration
5. Loop for 10k iterations while sleeping for 1ms after each iteration

Each iteration is craftered to take around 10s. More configurations
can be added as needed, but unfortunately the "5 phases" building block
can't be reused en masse because YAML doesn't support merging lists.

The primary metric recorded in this workload is the average duration
of each iteration.

This workload is intended to stress the Genny client itself, so should
be run with the smallest MongoDB setup.



## IndexStress
### Owner
Atlas Serverless
### Description
This workload is an extension of scale/LargeIndexedIns.yml to run the workload
on multiple serverless tenants, with some slight variations. The Clients are
expected to be overridden with URI keys to provide multiple connection strings.
On each cluster, the workload sends bursts of finds that use large $ins while
running a low rate update workload. The workload causes high CPU load as the
server becomes bottlenecked on tcmalloc spinlocks during the find operations.

In this workload, large arrays of random strings are generated to use
for the $in queries.  To avoid a CPU bottleneck on the workload client,
it uses a ^Once generator to generate the arrays once during initialization.

IMPORTANT NOTE: Refer to this wiki if you're changing the number of Clients in
this workload: https://tinyurl.com/ycyr45fs



## BatchedUpdateOneWithoutShardKeyWithId
### Owner
@mongodb/sharding
### Description
Runs the batched updateOne writes of type WithoutShardKeyWithId with a batch size of 1000.

The workload consists of 3 phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating the sharded collection with data.
  3. Running updateOne operations of type WriteWithoutShardKeyWithId.

The inserted documents have the following form:

    {_id: 10, oldKey: 20, newKey: 30, counter: 0, padding: 'random string of bytes to bring the docs up to 8 to 10 KB...'}

The collection is sharded on {oldKey: 'hashed'}. The metrics to watchout for here are P50, P99 operation latencies and overall throughput.

  
### Keywords
RunCommand, sharded, Loader, insert, update, updateOne, batch, latency 


## BulkWriteBatchedUpdateOneWithoutShardKeyWithId
### Owner
@mongodb/sharding
### Description
Runs the batched bulk updateOne writes of type WithoutShardKeyWithId with a batch size of 1000 using
bulkWrite command.

The workload consists of 3 phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating the sharded collection with data.
  3. Running updateOne writes of type WriteWithoutShardKeyWithId.

The inserted documents have the following form:

    {_id: 10, oldKey: 20, newKey: 30, counter: 0, padding: 'random string of bytes to bring the docs up to 8 to 10 KB...'}

The collection is sharded on {oldKey: 'hashed'}.  The metrics to watchout for here are P50, P99 operation latencies and overall throughput.

  
### Keywords
CrudActor, sharded, Loader, insert, update, updateOne, batch, bulkWrite, latency 


## DistinctCommands
### Owner
@mongodb/product-perf
### Description
This workload tests distinct commands with large strings to repro SERVER-43096.
The main metrics to look at for this test are the throughput for the finds and
distincts that are run during the test. After SERVER-43096, the throughput
is much higher for both of these actors.

  
### Keywords
Distinct, Large Strings 


## MongosMerging
### Owner
@mongodb/product-perf
### Description
This workload runs different types of aggregations where the query results
will be merged on a mongos node. This workload reproduces a SERVER-29446,
and results in an error when run that says the $sample stage could not
find a non-duplicate document.

  
### Keywords
Aggregations, Mongos, Sample, Unwind, Sort 


## MultiShardTransactions
### Owner
@mongodb/sharding
### Description



## MultiShardTransactionsWithManyNamespaces
### Owner
@mongodb/sharding
### Description



## ReshardCollection
### Owner
@mongodb/sharding
### Description
Runs the reshardCollection command while read and write operations are active on the collection
being resharded.

The workload consists of 5 phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating the sharded collection with data.
  3. Running read and write operations on the collection before it is resharded.
  4. Running read and write operations on the collection while it is being resharded.
  5. Running read and write operations on the collection after it has been resharded.

The inserted documents have the following form:

    {_id: 10, oldKey: 20, newKey: 30, counter: 0, padding: 'random string of bytes ...'}

The collection is initially sharded on {oldKey: 'hashed'} and then resharded on {newKey: 1}.



## ReshardCollectionMixed
### Owner
@mongodb/sharding
### Description
Runs the reshardCollection command while read and write operations are active on the collection
being resharded.

The workload consists of 5 phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating the sharded collection with data.
  3. Running read and write operations on the collection before it is resharded.
  4. Running read and write operations on the collection while it is being resharded.
  5. Running read and write operations on the collection after it has been resharded.

The inserted documents have the following form:

    {_id: 10, oldKey: 20, newKey: 30, counter: 0, padding: 'random string of bytes ...'}

The collection is initially sharded on {oldKey: 'hashed'} and then resharded on {newKey: 1}.



## ReshardCollectionReadHeavy
### Owner
@mongodb/sharding
### Description
Runs the reshardCollection command while read and write operations are active on the collection
being resharded.

The workload consists of 5 phases:
  1. Creating an empty sharded collection distributed across all shards in the cluster.
  2. Populating the sharded collection with data.
  3. Running read and write operations on the collection before it is resharded.
  4. Running read and write operations on the collection while it is being resharded.
  5. Running read and write operations on the collection after it has been resharded.

The inserted documents have the following form:

    {_id: 10, oldKey: 20, newKey: 30, counter: 0, padding: 'random string of bytes ...'}

The collection is initially sharded on {oldKey: 'hashed'} and then resharded on {newKey: 1}.



## ReshardCollectionWithIndexes
### Owner
@mongodb/sharding
### Description
This test measures the time for a sharded cluster to reshard a collection from one shard to two 
shards then to three shards. It was added August 2023 as part of PM-2322, to demonstrate planned 
resharding performance improvements. Note that the goal of this test is to show the performance 
gain on this setup rather than the performance difference on different kinds of data type.

The test expects the target cluster is created using ebs snapshot with 1 billion 1KB records and 
has 3 shards, named shard-00, shard-01, shard-02. The collection has 10 indexes including 
_id index, single-key index and compound index. The whole dataset is on 1 shard at the beginning.
ReshardCollection should use same-key resharding and use shardDistribution parameter to reshard 
into 2 shards then 3 shards. There will be random CRUD operations during resharding but should 
run at a very low rate.

The workload consists of 3 phases:
  1. Turning off balancer and make the sharded collection exists on only 1 shard.
  2. Running read and write operations on the collection while it is being resharded to 2 shards.
  3. Running read and write operations on the collection while it is being resharded to 3 shards.

All documents are generated through genny' data loader where the integer and length of short 
string fields are randomly generated. The fields are designed like to form different combinations 
of indexes. The goal of having 10 indexes is to test resharding performance with indexes and the 
number 10 comes from design, which is arbitrary from testing perspective.
  
The inserted documents have the following form:

    {
      _id: integer(default _id index),
      shardKey: integer(for the shard key),
      counter: integer(used for counting updates),
      num1: integer(random generated between [1, 1000000000]),
      num2: integer(random generated between [1, 1000000000]),
      str1: string(length <= 20),
      str2: string(length <= 20),
      padding: string(random string of bytes ...)
    }

The indexes are:
    [
      {_id: 1},
      {shardKey: 1},
      {counter: 1},
      {num1: 1},
      {num2: 1},
      {str1: 1},
      {str2: 1},
      {shardKey: 1, counter:1},
      {str1: 1, num1: 1},
      {num2:1, str2: 1}
    ]

  
### Keywords
resharding, indexes, replication, collection copy 


## WouldChangeOwningShardBatchWrite
### Owner
@mongodb/sharding
### Description
Creates a sharded collection with 2 chunks on 2 different shards using ranged sharding and updates
the shard key to trigger WouldChangeOwningShard errors.

The workload consists of 3 phases:
  1. Shard an empty collection (using ranged sharding) spreading 2 chunks across 2 shards.
  2. Populate the sharded collection with data.
  3. Update the shard key value to trigger WouldChangeOwningShard errors.



## WriteOneReplicaSet
### Owner
@mongodb/sharding
### Description
Run updateOnes, deleteOnes, and findAndModifys on a replica set.



## WriteOneWithoutShardKeyShardedCollection
### Owner
@mongodb/sharding
### Description
On a sharded collection on a single shard cluster, run workloads that updateOne, deleteOne, and findAndModify.



## WriteOneWithoutShardKeyUnshardedCollection
### Owner
@mongodb/sharding
### Description
On an unsharded collection on a single shard cluster, run workloads that updateOne, deleteOne, and findAndModify.



## MultiUpdates-PauseMigrations-ShardCollection
### Owner
@mongodb/sharding
### Description
See phases/sharding/multi_updates/MultiUpdatesTemplate.yml.


## MultiUpdates-PauseMigrations
### Owner
@mongodb/sharding
### Description
See phases/sharding/multi_updates/MultiUpdatesTemplate.yml.


## MultiUpdates-ShardCollection
### Owner
@mongodb/sharding
### Description
See phases/sharding/multi_updates/MultiUpdatesTemplate.yml.


## MultiUpdates
### Owner
@mongodb/sharding
### Description
See phases/sharding/multi_updates/MultiUpdatesTemplate.yml.


## AddFields
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> ... 20 $addField ... -> Tumbling Window
Input Documents: 8M
BatchSize: 1k

Simulates a long pipeline with 20 $addField stages funneled into a tumbling window.
The goal is to test the performance of both $addField and streaming pipelines with
many stages.

  
### Keywords
streams 


## LargeHoppingWindow
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Hopping Window (Group) -> Memory
Input Documents: 1.6M
BatchSize: 1k

Simulates the scenario where the input to output ratio is 1 to 4 using a hopping window. The
hopping window has an interval of one second with a hop size of 250ms, so each document gets
consumed into four different windows. Each of the ingested 1.6M documents have a unique group key,
so this will produce 6.4M output documents for the 1.6M input documents. The first part of this
workload tests the ingestion throughput when the window is open and never closes, the second part
of this workload tests the flush throughput when the window is closed and all the group documents
are flushed to the sink.

  
### Keywords
streams 


## LargeTumblingWindow
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Tumbling Window (Group) -> Memory
Input Documents: 3.2M
BatchSize: 1k

Simulates the scenario where the input to output ratio is 1 to 1 using a tumbling window. The
tumbling window has an interval of one second. Each of the ingested 3.2M documents have a unique
group key, so this will produce 3.2M output documents for the 3.2M input documents. The first part
of this workload tests the ingestion throughput when the window is open and never closes, the second
part of this workload tests the flush throughput when the window is closed and all the group documents
are flushed to the sink.

  
### Keywords
streams 


## LargeWindowMixed
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Tumbling Window (Group) -> Memory
Input Documents: 16M
BatchSize: 1k

Simulates the scenario where the watermark is constantly advancing every some documents
and constantly opening new windows and closing old window over the watermark span of 10
seconds on a tumbling window with an interval of 1 second and an allow lateness of 1 second.
Each window will ingest 1.6M documents, with ~400k unique keys, so each window on close will
output atmost 400k documents.

  
### Keywords
streams 


## LargeWindowUniqueAndExistingKeys
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Tumbling Window (Group) -> Memory
Documents: 16M
BatchSize: 1k

Simulates the scenario where only unique keys are inserted into a window group operator, and then
simulates the scenario where only existing keys are inserted in the same window group oeprator.
The first 8M documents will all have unique auction IDs and will measure the performance of the
scenario where every document results in inserting a new key into the window. The latter 8M
documents will all have an existing auction ID will measure the performance of the scenario where
every document results in updating an existing key in the window.

  
### Keywords
streams 


## Passthrough
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Project -> Memory
Documents: 8M
BatchSize: 1k

Simulates the scenario where the input and output of documents for a stream processor is a
one-to-one ratio. This applies a simple projection on incoming documents (currency conversion).

  
### Keywords
streams 


## Passthrough_ChangeStreamSource
### Owner
@10gen/altas-streams
### Description
Pipeline: Mongo Change Stream -> Project -> Memory
Documents: 800k
BatchSize: 1k

Simulates the scenario where the input and output of documents for a stream processor is a
one-to-one ratio. This applies a simple projection on incoming documents (currency conversion).
The difference with this workload versus Passthrough.yml is that this uses a mongo change stream
as the source rather than the in-memory source operator.

  
### Keywords
streams 


## Passthrough_MongoSink
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Project -> MongoDB
Documents: 800k
BatchSize: 1k

This tests the scenario where the input and output of documents for a stream processor is a
one-to-one ratio. This applies a simple projection on incoming documents (currency conversion).
The difference with this workload versus Passthrough.yml is that this uses a $merge MongoDB sink
rather than the no-op sink operator.

  
### Keywords
streams 


## Search
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Match -> Project -> Memory
Input Documents: 8M
BatchSize: 1k

Simulates the scenario where the stream processor only needs to match a
small portion of the ingested documents. In this specific case, 8M documents
are ingested but the $match stage will only match against ~0.3% of the ingested
documents.

  
### Keywords
streams 


## StreamsLookup
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Lookup -> AddField -> Window (Group) -> Memory
Input Documents: 8M
BatchSize: 1k
ForeignCollectionDocuments: 10k

Simulates the scenario where incoming data needs to be merged with a foreign mongoDB collection
and then propagated to a tumbling window which groups by a foreign column that was fetched
from the $lookup (join) on the foreign mongoDB collection.

  
### Keywords
streams 


## TopKPerWindow
### Owner
@10gen/altas-streams
### Description
Pipeline: Memory -> Tumbling Window (Group, Sort, Limit 1k) -> Memory
Input Documents: 8M
BatchSize: 1k

Simulates the scenario where input documents are fed into a tumlbing window that
groups by a large key (URL), then sorts by the aggregated price, and only emits
the top 1k aggregated documents by price. All 8M documents will have a random URL
assigned to them.

  
### Keywords
streams 


## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 against the denormalized schema for scale 1.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 against the denormalized schema for scale 1.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 against the denormalized schema for scale 1.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 against the denormalized schema for scale 1.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 against the denormalized schema for scale 1.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 against the denormalized schema for scale 1.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 against the denormalized schema for scale 1.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 against the denormalized schema for scale 1.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 against the denormalized schema for scale 1.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 against the denormalized schema for scale 1.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 against the denormalized schema for scale 1.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 against the denormalized schema for scale 1.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 against the denormalized schema for scale 1.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 against the denormalized schema for scale 1.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 against the denormalized schema for scale 1.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 against the denormalized schema for scale 1.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 against the denormalized schema for scale 1.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 against the denormalized schema for scale 1.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 against the denormalized schema for scale 1.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 against the denormalized schema for scale 1.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 against the denormalized schema for scale 1.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 against the denormalized schema for scale 1.



## validate
### Owner
@mongodb/product-query
### Description
Validate TPC_H denormalized queries for scale 1. Note that numeric comparison is not exact in this workload;
the AssertiveActor only ensures that any two values of numeric type are approximately equal according to a hard-coded limit.



## AvgAcctBal
### Owner
@mongodb/product-query
### Description
Run an artifical TPC-H query to get the average customer account balance against the denormalized
schema for scale 10.



## AvgItemCost
### Owner
@mongodb/product-query
### Description
Run an artifical TPC-H query to get the average cost of item sold against the denormalized schema
for scale 10.



## BiggestOrders
### Owner
@mongodb/product-query
### Description
Run an artifical TPC-H query to get the biggest EUROPE orders against the denormalized schema for
scale 10.



## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 against the denormalized schema for scale 10.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 against the denormalized schema for scale 10.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 against the denormalized schema for scale 10.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 against the denormalized schema for scale 10.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 against the denormalized schema for scale 10.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 against the denormalized schema for scale 10.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 against the denormalized schema for scale 10.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 against the denormalized schema for scale 10.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 against the denormalized schema for scale 10.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 against the denormalized schema for scale 10.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 against the denormalized schema for scale 10.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 against the denormalized schema for scale 10.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 against the denormalized schema for scale 10.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 against the denormalized schema for scale 10.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 against the denormalized schema for scale 10.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 against the denormalized schema for scale 10.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 against the denormalized schema for scale 10.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 against the denormalized schema for scale 10.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 against the denormalized schema for scale 10.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 against the denormalized schema for scale 10.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 against the denormalized schema for scale 10.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 against the denormalized schema for scale 10.



## TotalLineitemRevenue
### Owner
@mongodb/product-query
### Description
Run an artifical TPC-H query to sum up total price across all lineitems against the denormalized
schema for scale 10.



## TotalOrderRevenue
### Owner
@mongodb/product-query
### Description
Run an artifical TPC-H query to sum up total price across all orders against the denormalized
schema for scale 10.



## validate
### Owner
@mongodb/product-query
### Description
The test control for TPC-H expects a validate.yml file to exist in all TPC-H scales.
We don't need validation on scale 10, so this is just a Nop.



## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 against the normalized schema for scale 1.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 against the normalized schema for scale 1.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 against the normalized schema for scale 1.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 against the normalized schema for scale 1.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 against the normalized schema for scale 1.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 against the normalized schema for scale 1.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 against the normalized schema for scale 1.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 against the normalized schema for scale 1.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 against the normalized schema for scale 1.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 against the normalized schema for scale 1.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 against the normalized schema for scale 1.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 against the normalized schema for scale 1.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 against the normalized schema for scale 1.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 against the normalized schema for scale 1.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 against the normalized schema for scale 1.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 against the normalized schema for scale 1.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 against the normalized schema for scale 1.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 against the normalized schema for scale 1.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 against the normalized schema for scale 1.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 against the normalized schema for scale 1.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 against the normalized schema for scale 1.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 against the normalized schema for scale 1.



## validate
### Owner
@mongodb/product-query
### Description
Validate TPC_H normalized queries for scale 1. Note that numeric comparison is not exact in this workload;
the AssertiveActor only ensures that any two values of numeric type are approximately equal according to a hard-coded limit.



## Q1
### Owner
@mongodb/product-query
### Description
Run TPC-H query 1 against the normalized schema for scale 10.



## Q10
### Owner
@mongodb/product-query
### Description
Run TPC-H query 10 against the normalized schema for scale 10.



## Q11
### Owner
@mongodb/product-query
### Description
Run TPC-H query 11 against the normalized schema for scale 10.



## Q12
### Owner
@mongodb/product-query
### Description
Run TPC-H query 12 against the normalized schema for scale 10.



## Q13
### Owner
@mongodb/product-query
### Description
Run TPC-H query 13 against the normalized schema for scale 10.



## Q14
### Owner
@mongodb/product-query
### Description
Run TPC-H query 14 against the normalized schema for scale 10.



## Q15
### Owner
@mongodb/product-query
### Description
Run TPC-H query 15 against the normalized schema for scale 10.



## Q16
### Owner
@mongodb/product-query
### Description
Run TPC-H query 16 against the normalized schema for scale 10.



## Q17
### Owner
@mongodb/product-query
### Description
Run TPC-H query 17 against the normalized schema for scale 10.



## Q18
### Owner
@mongodb/product-query
### Description
Run TPC-H query 18 against the normalized schema for scale 10.



## Q19
### Owner
@mongodb/product-query
### Description
Run TPC-H query 19 against the normalized schema for scale 10.



## Q2
### Owner
@mongodb/product-query
### Description
Run TPC-H query 2 against the normalized schema for scale 10.



## Q20
### Owner
@mongodb/product-query
### Description
Run TPC-H query 20 against the normalized schema for scale 10.



## Q21
### Owner
@mongodb/product-query
### Description
Run TPC-H query 21 against the normalized schema for scale 10.



## Q22
### Owner
@mongodb/product-query
### Description
Run TPC-H query 22 against the normalized schema for scale 10.



## Q3
### Owner
@mongodb/product-query
### Description
Run TPC-H query 3 against the normalized schema for scale 10.



## Q4
### Owner
@mongodb/product-query
### Description
Run TPC-H query 4 against the normalized schema for scale 10.



## Q5
### Owner
@mongodb/product-query
### Description
Run TPC-H query 5 against the normalized schema for scale 10.



## Q6
### Owner
@mongodb/product-query
### Description
Run TPC-H query 6 against the normalized schema for scale 10.



## Q7
### Owner
@mongodb/product-query
### Description
Run TPC-H query 7 against the normalized schema for scale 10.



## Q8
### Owner
@mongodb/product-query
### Description
Run TPC-H query 8 against the normalized schema for scale 10.



## Q9
### Owner
@mongodb/product-query
### Description
Run TPC-H query 9 against the normalized schema for scale 10.



## validate
### Owner
@mongodb/product-query
### Description
The test control for TPC-H expects a validate.yml file to exist in all TPC-H scales.
We don't need validation on scale 10, so this is just a Nop.



## LLTAnalytics
### Owner
@mongodb/product-perf
### Description
Workload to Benchmark the effect of LongLivedTransactions on an Update workload.

  
### Keywords
transactions, long lived, snapshot, update 


## LLTMixed
### Owner
@mongodb/product-perf
### Description
Workload to Benchmark the effect of LongLivedTransactions on a Mixed workload.
The intent here is to test multiple transactions contained within a singular transaction (long lived).
The transactions are setup such that they cover a mixed set of operations (insert, query, update, remove).
The operations are also divided in the length of their run (short, medium, long) and whether they contain scans or not.
The workload has the following configuration:
~12GB dataset
10,000,000 documnets in total
Warm up phase, then operation phase followed by a quiesce phase
Naming Conventions:
Duration.Load_level.Operation.Type_of_test
Operation:     Insert|Query|Update|Remove|Mixed
Duration:      Short|Medium|Long
Type of test:  Baseline|Benchmark
Baseline without scans, benchmark with scans

  
### Keywords
transactions, longLived, snapshot, insert, find, update, delete 


## LLTMixedSmall
### Owner
@mongodb/product-perf
### Description
Workload to Benchmark the effect of LongLivedTransactions on a Mixed workload.
The intent here is to test multiple transactions contained within a singular transaction (long lived).
The transactions are setup such that they cover a mixed set of operations (insert, query, update, remove).
The operations are also divided in the length of their run (short, medium, long) and whether they contain scans or not.
The workload has the following configuration:
~7GB dataset
6,000,000 documnets in total
Warm up phase, then operation phase followed by a quiesce phase
Naming Conventions:
Duration.Load_level.Operation.Type_of_test
Operation:     Insert|Query|Update|Remove|Mixed
Duration:      Short|Medium|Long
Type of test:  Baseline|Benchmark
Baseline without scans, benchmark with scans


