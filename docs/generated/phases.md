# Phase Documentation

This documentation is generated from the yaml files in the `src/phases` directory. The phases listed here are limited in scope to the Genny repo.

We parse the `keywords`, `owner`, `description`, and the phase name from each yaml file to generate this documentation. Clicking on the header of each phase will take you to its yaml file in the repo.

If you want to update the documentation please update the phase's respective yaml file, run `./run-genny generate-docs`, and commit the changes.


## [ExamplePhase2](https://www.github.com/mongodb/genny/blob/master/src/phases/HelloWorld/ExamplePhase2.yml)
### Owner
Product Performance


### Support Channel
[#performance](https://mongodb.enterprise.slack.com/archives/C0V3KSB52)


### Description
Example phase to illustrate how PhaseConfig composition works.





## [ContinuousWritesWithExponentialCompactTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/encrypted/ContinuousWritesWithExponentialCompactTemplate.yml)
### Owner
Server Security


### Support Channel
[#server-security](https://mongodb.enterprise.slack.com/archives/CB3CW8M8C)


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





## [YCSBLikeActorTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/encrypted/YCSBLikeActorTemplate.yml)
### Owner
Server Security


### Support Channel
[#server-security](https://mongodb.enterprise.slack.com/archives/CB3CW8M8C)


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





## [YCSBLikeEncryptedTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/encrypted/YCSBLikeEncryptedTemplate.yml)
### Owner
Server Security


### Support Channel
[#server-security](https://mongodb.enterprise.slack.com/archives/CB3CW8M8C)


### Description
Template for encryption-enabled workloads that emulate a YCSB workload.
Performs queries on an encrypted field, instead of _id, during the read/update phase.
Parameters:
  Database:             name of encrypted database
  Collection:           name of encrypted collection
  ClientName:           name of encrypted client pool
  Field<n>Value:        value of the nth field (n is 1..8)





## [ClusteredCollection](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/ClusteredCollection.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
Run basic insert and find and delete workload on a collection clustered by {_id: 1} .
Clustered collections are planned to mainly serve operations over the cluster
key such as monotonically increasing inserts, range queries and range deletions.





## [CreateIndexPhase](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/CreateIndexPhase.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
This Phase has 2 actors, InsertData and IndexCollection. InsertData inserts documents containing
all types of indexes, and IndexCollection creates indexes for each of them, one at a time.





## [MixedMultiDeletes](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/MixedMultiDeletes.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


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


## [TimeSeriesUpdatesAndDeletes](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/TimeSeriesUpdatesAndDeletes.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


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





## [UpdateWithSecondaryIndexes](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/UpdateWithSecondaryIndexes.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
Runs a workload that updates a large range of documents.
Multiple secondary indexes are present.
Update performed with and without a hint.




### Keywords
RunCommand, Loader, LoggingActor, CrudActor, insert, update, latency


## [ValidateCmd](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/ValidateCmd.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
This workload inserts ~1GB of documents, creates various indexes on the data, and then runs the
validate command. We created this workload to see the performance benefits of improvements
to the validate command, including background validation.





## [Default](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/config/MultiDeletes/Default.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
Configuration for the MultiDeletes workload.





## [WithSecondaryIndexes](https://www.github.com/mongodb/genny/blob/master/src/phases/execution/config/MultiDeletes/WithSecondaryIndexes.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
Configuration for MultiDeletes workload. This configuration introduces secondary indexes.





## [AggregateExpressions](https://www.github.com/mongodb/genny/blob/master/src/phases/query/AggregateExpressions.yml)
### Owner
@mongodb/query



### Description
This file defines a template to use in aggregation expression performance tests.





## [BooleanSimplifier](https://www.github.com/mongodb/genny/blob/master/src/phases/query/BooleanSimplifier.yml)
### Owner
Query Optimization


### Support Channel
[#query-optimization](https://mongodb.enterprise.slack.com/archives/CQCBTN138)


### Description
This workload measures performance of boolean expressions which can be simplified by
the Boolean Simplifier. It is designed to track effectiveness of the simplifier.





## [CollScanComplexPredicate](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanComplexPredicate.yml)
### Owner
@mongodb/query



### Description
This workload tests the performance of collection scan queries with complex predicates of
various shapes (CNF, DNF, and mixed predicates with different levels of nestedness).





## [CollScanComplexPredicateQueries](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanComplexPredicateQueries.yml)
### Owner
@mongodb/query



### Description
Defines complex, randomly generated CNF and DNF queries used in 'CollScanComplexPredicate.yml'.





## [CollScanLargeNumberOfFields](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanLargeNumberOfFields.yml)
### Owner
@mongodb/query



### Description
This workload tests the performance of collection scan queries against a collection containing
documents with a large number of fields.




### Keywords
Loader, CrudActor, QuiesceActor, insert, find


## [CollScanOnMixedDataTypes](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanOnMixedDataTypes.yml)
### Owner
@mongodb/query



### Description
This workload runs collscan queries on various data types.





## [CollScanPredicateSelectivity](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanPredicateSelectivity.yml)
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


## [CollScanProjection](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanProjection.yml)
### Owner
@mongodb/query



### Description
This workload runs collscan queries with a large projection on around 20 fields.





## [CollScanSimplifiablePredicate](https://www.github.com/mongodb/genny/blob/master/src/phases/query/CollScanSimplifiablePredicate.yml)
### Owner
@mongodb/query



### Description
This workload tests the performance of collection scan queries with complex predicates
that can be simplified by the optimizer.





## [FilterWithComplexLogicalExpression](https://www.github.com/mongodb/genny/blob/master/src/phases/query/FilterWithComplexLogicalExpression.yml)
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


## [GetBsonDate](https://www.github.com/mongodb/genny/blob/master/src/phases/query/GetBsonDate.yml)
### Owner
Query Execution


### Support Channel
[#query-execution](https://mongodb.enterprise.slack.com/archives/CKABWR2CT)


### Description
This file defines a parameterized configuration 'GetBsonDate' to work around the issue of
ISODate.

TODO PERF-3132 Use the date generator instead of this workaround.





## [GroupStagesOnComputedFields](https://www.github.com/mongodb/genny/blob/master/src/phases/query/GroupStagesOnComputedFields.yml)
### Owner
@mongodb/query



### Description
This file defines templates to use in workloads exercising aggregate stages on computed
time fields.





## [IDHack](https://www.github.com/mongodb/genny/blob/master/src/phases/query/IDHack.yml)
### Owner
@mongodb/query



### Description
Defines common configurations for IDHack query workloads.





## [LookupCommands](https://www.github.com/mongodb/genny/blob/master/src/phases/query/LookupCommands.yml)
### Owner
Query Execution


### Support Channel
[#query-execution](https://mongodb.enterprise.slack.com/archives/CKABWR2CT)


### Description
Defines common configurations for 'LookupSBEPushdown' and 'LookupSBEPushdownMisc' workloads.





## [MatchFilters](https://www.github.com/mongodb/genny/blob/master/src/phases/query/MatchFilters.yml)
### Owner
@mongodb/query



### Description
This workload tests a set of filters in the match language. The actors below offer basic
performance coverage for said filters.




### Keywords
Loader, CrudActor, QuiesceActor, insert, find


## [Multiplanner](https://www.github.com/mongodb/genny/blob/master/src/phases/query/Multiplanner.yml)
### Owner
@mongodb/query



### Description
This file defines templates to use in multiplanner performance tests.





## [RepeatedPathTraversal](https://www.github.com/mongodb/genny/blob/master/src/phases/query/RepeatedPathTraversal.yml)
### Owner
@mongodb/query



### Description
This workload stresses the query execution engine by running queries over a set of paths which
share a common prefix. Crucially, these queries never match a document in the collection.




### Keywords
Loader, CrudActor, QuiesceActor, insert, find


## [RunLargeArithmeticOp](https://www.github.com/mongodb/genny/blob/master/src/phases/query/RunLargeArithmeticOp.yml)
### Owner
@mongodb/query



### Description
This phase template constructs an aggregation pipeline that multiplies together
the provided arguments.





## [TimeSeriesLastpoint](https://www.github.com/mongodb/genny/blob/master/src/phases/query/TimeSeriesLastpoint.yml)
### Owner
@mongodb/query



### Description
These are the phases used to measure performance of the last-point-in-time query optimization on timeseries collections.





## [TimeSeriesSortCommands](https://www.github.com/mongodb/genny/blob/master/src/phases/query/TimeSeriesSortCommands.yml)
### Owner
@mongodb/query



### Description
These are the phases used to measure performance of the bounded sorter for timeseries collections.





## [Views](https://www.github.com/mongodb/genny/blob/master/src/phases/query/Views.yml)
### Owner
@mongodb/query



### Description
Defines common configurations for workloads that operate on views.





## [StartupPhasesTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/replication/startup/StartupPhasesTemplate.yml)
### Owner
Replication


### Support Channel
[#server-replication](https://mongodb.enterprise.slack.com/archives/C0V7X00AD)


### Description
Common definitions to support the workloads in replication/startup.





## [DesignDocWorkloadPhases](https://www.github.com/mongodb/genny/blob/master/src/phases/scale/DesignDocWorkloadPhases.yml)
### Owner
Performance Infrastructure


### Support Channel
[#ask-devprod-performance](https://mongodb.enterprise.slack.com/archives/C01VD0LQZED)


### Description
Common definitions to support the workloads in scale/LargeScale. See
LargeScaleSerial.yml for a general overview of what the large-scale workloads are
testing.

Note that there aren't any performance targets defined - these were moved to an
out-of-band tracking system that automatically spots regressions across all recorded
measurements.





## [LargeScalePhases](https://www.github.com/mongodb/genny/blob/master/src/phases/scale/LargeScalePhases.yml)
### Owner
Storage Execution


### Support Channel
[#server-storage-execution](https://mongodb.enterprise.slack.com/archives/C2RCHGB2L)


### Description
This is the set of shared phases for the Large Scale Workload Automation project.

Since the LSWA workloads contain common phases, they've been separated in this phase file and can
be included in each workload as needed via the `ExternalPhaseConfig` functionality.





## [MixPhases](https://www.github.com/mongodb/genny/blob/master/src/phases/scale/MixPhases.yml)
### Owner
Product Performance


### Support Channel
[#performance](https://mongodb.enterprise.slack.com/archives/C0V3KSB52)


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


## [SetClusterParameterTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/sharding/SetClusterParameterTemplate.yml)
### Owner
Cluster Scalability


### Support Channel
[#server-sharding](https://mongodb.enterprise.slack.com/archives/C8PK5KZ5H)


### Description
Template for setting a cluster parameter.





## [ShardCollectionTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/sharding/ShardCollectionTemplate.yml)
### Owner
Cluster Scalability


### Support Channel
[#server-sharding](https://mongodb.enterprise.slack.com/archives/C8PK5KZ5H)


### Description
Template for sharding a collection.





## [MultiUpdatesTemplate](https://www.github.com/mongodb/genny/blob/master/src/phases/sharding/multi_updates/MultiUpdatesTemplate.yml)
### Owner
Cluster Scalability


### Support Channel
[#server-sharding](https://mongodb.enterprise.slack.com/archives/C8PK5KZ5H)


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


## [Q1](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q1.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 1 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q10](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q10.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 10 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q11](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q11.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 11 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q12](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q12.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 12 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q13](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q13.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 13 gainst the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q14](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q14.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 14 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q15](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q15.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 15 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q16](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q16.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 16 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q17](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q17.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 17 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q18](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q18.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 18 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q19](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q19.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 19 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q2](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q2.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 2 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q20](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q20.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 20 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q21](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q21.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 21 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q22](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q22.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 22 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q3](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q3.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 3 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q4](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q4.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 4 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q5](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q5.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 5 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q6](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q6.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 6 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q7](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q7.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 7 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q8](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q8.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 8 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q9](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/denormalized/Q9.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 9 against the denormalized schema. Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q1](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q1.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 1 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q10](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q10.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 10 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q11](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q11.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 11 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q12](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q12.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 12 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q13](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q13.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 13 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q14](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q14.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 14 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q15](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q15.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 15 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q16](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q16.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 16 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q17](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q17.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 17 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q18](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q18.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 18 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q19](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q19.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 19 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q2](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q2.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 2 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.





## [Q20](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q20.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 20 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q21](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q21.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 21 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q22](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q22.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 22 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q3](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q3.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 3 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q4](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q4.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 4 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q5](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q5.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 5 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q6](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q6.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 6 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q7](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q7.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 7 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q8](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q8.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 8 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.





## [Q9](https://www.github.com/mongodb/genny/blob/master/src/phases/tpch/normalized/Q9.yml)
### Owner
@mongodb/product-query



### Description
Run TPC-H query 9 (see http://tpc.org/tpc_documents_current_versions/pdf/tpc-h_v3.0.0.pdf) against the normalized schema.
Using an 'executionStats' explain causes each command to run its execution plan until no
documents remain, which ensures that the query executes in its entirety.




