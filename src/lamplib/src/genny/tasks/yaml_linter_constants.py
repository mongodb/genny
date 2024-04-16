import os

# These fields are required in every workload file.
REQUIRED_FIELDS = ["Description", "Owner", "Keywords"]

# These workloads are grandfathered in and don't need to be linted for the keywords field. See DEVPROD-5930.
GRANDFATHERED_WORKLOADS_WITHOUT_KEYWORDS = set(
    os.path.join(os.getenv("GENNY_REPO_ROOT", ""), path)
    for path in [
        "src/workloads/selftests/GennyOverhead.yml",
        "src/workloads/scale/LargeIndexedIns.yml",
        "src/workloads/scale/InCacheSnapshotReads.yml",
        "src/workloads/scale/OutOfCacheSnapshotReads.yml",
        "src/workloads/scale/MajorityReads10KThreads.yml",
        "src/workloads/scale/LoadTest.yml",
        "src/workloads/scale/ScanWithLongLived.yml",
        "src/workloads/scale/LargeScaleModel.yml",
        "src/workloads/scale/MajorityWrites10KThreads.yml",
        "src/workloads/scale/MixedWrites.yml",
        "src/workloads/scale/OutOfCacheScanner.yml",
        "src/workloads/scale/InsertBigDocs.yml",
        "src/workloads/scale/LargeScaleLongLived.yml",
        "src/workloads/scale/TimeSeriesSortScale.yml",
        "src/workloads/scale/MassDeleteRegression.yml",
        "src/workloads/serverless/IndexStress.yml",
        "src/workloads/networking/TransportLayerConnectTiming.yml",
        "src/workloads/networking/ServiceArchitectureWorkloads.yml",
        "src/workloads/networking/SecondaryAllowed.yml",
        "src/workloads/c2c/MongosyncScripts.yml",
        "src/workloads/sharding/ReshardCollection.yml",
        "src/workloads/sharding/WouldChangeOwningShardBatchWrite.yml",
        "src/workloads/sharding/MultiShardTransactions.yml",
        "src/workloads/sharding/ReshardCollectionMixed.yml",
        "src/workloads/sharding/WriteOneReplicaSet.yml",
        "src/workloads/sharding/WriteOneWithoutShardKeyShardedCollection.yml",
        "src/workloads/sharding/ReshardCollectionReadHeavy.yml",
        "src/workloads/sharding/WriteOneWithoutShardKeyUnshardedCollection.yml",
        "src/workloads/sharding/MultiShardTransactionsWithManyNamespaces.yml",
        "src/workloads/sharding/multi_updates/MultiUpdates-ShardCollection.yml",
        "src/workloads/sharding/multi_updates/MultiUpdates.yml",
        "src/workloads/sharding/multi_updates/MultiUpdates-PauseMigrations-ShardCollection.yml",
        "src/workloads/sharding/multi_updates/MultiUpdates-PauseMigrations.yml",
        "src/workloads/docs/HelloWorld-MultiplePhases.yml",
        "src/workloads/docs/SamplingLoader.yml",
        "src/workloads/docs/CrudActorFSM.yml",
        "src/workloads/docs/LongLivedWriter.yml",
        "src/workloads/docs/HotCollectionWriter.yml",
        "src/workloads/docs/GeneratorsSeeded.yml",
        "src/workloads/docs/StreamStatsReporter.yml",
        "src/workloads/docs/QuiesceActor.yml",
        "src/workloads/docs/LongLivedReader.yml",
        "src/workloads/docs/RollingCollections.yml",
        "src/workloads/docs/ExternalScriptActor.yml",
        "src/workloads/docs/CrudActor.yml",
        "src/workloads/docs/RunCommand.yml",
        "src/workloads/docs/CrudFSMTrivial.yml",
        "src/workloads/docs/HelloWorld-LoadConfig.yml",
        "src/workloads/docs/CrudActorTransaction.yml",
        "src/workloads/docs/CrudActorAggregate.yml",
        "src/workloads/docs/HelloWorld.yml",
        "src/workloads/docs/RunCommand-Simple.yml",
        "src/workloads/docs/ParallelInsert.yml",
        "src/workloads/docs/HotDocumentWriter.yml",
        "src/workloads/docs/Deleter.yml",
        "src/workloads/docs/ChooseFromDataset.yml",
        "src/workloads/docs/Generators.yml",
        "src/workloads/docs/LongLivedCreator.yml",
        "src/workloads/docs/RandomSampler.yml",
        "src/workloads/docs/CollectionScanner.yml",
        "src/workloads/docs/HelloWorld-ActorTemplate.yml",
        "src/workloads/docs/CrudActorEncrypted.yml",
        "src/workloads/docs/CrudActorFSMAdvanced.yml",
        "src/workloads/docs/LoggingActorExample.yml",
        "src/workloads/contrib/qe_test_gen/maps_medical.yml",
        "src/workloads/contrib/qe_test_gen/patchConfig.yml",
        "src/workloads/contrib/historystore/eMRCfPopulate.yml",
        "src/workloads/contrib/historystore/eMRCfBench.yml",
        "src/workloads/contrib/historystore/eMRCfGrow.yml",
        "src/workloads/transactions/LLTMixedSmall.yml",
        "src/workloads/tpch/normalized/1/Q7_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q18_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q19_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q6_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q4_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q5_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q1_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q22_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q2_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q21_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q20_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q3_normalized_1.yml",
        "src/workloads/tpch/normalized/1/validate_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q11_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q10_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q12_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q13_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q8_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q17_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q16_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q9_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q14_normalized_1.yml",
        "src/workloads/tpch/normalized/1/Q15_normalized_1.yml",
        "src/workloads/tpch/normalized/10/Q7_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q18_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q19_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q6_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q4_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q5_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q1_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q22_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q2_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q21_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q20_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q3_normalized_10.yml",
        "src/workloads/tpch/normalized/10/validate_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q11_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q10_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q12_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q13_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q8_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q17_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q16_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q9_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q14_normalized_10.yml",
        "src/workloads/tpch/normalized/10/Q15_normalized_10.yml",
        "src/workloads/tpch/denormalized/1/Q7_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q18_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q19_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q6_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q4_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q5_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q1_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q22_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q2_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q21_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q20_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q3_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/validate_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q11_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q10_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q12_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q13_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q8_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q17_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q16_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q9_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q14_denormalized_1.yml",
        "src/workloads/tpch/denormalized/1/Q15_denormalized_1.yml",
        "src/workloads/tpch/denormalized/10/TotalLineitemRevenue_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q7_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q18_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q19_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q6_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/AvgAcctBal_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q4_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q5_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q1_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q22_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/BiggestOrders_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q2_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q21_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q20_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q3_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/validate_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q11_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q10_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q12_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q13_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q8_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/TotalOrderRevenue_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q17_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q16_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q9_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q14_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/Q15_denormalized_10.yml",
        "src/workloads/tpch/denormalized/10/AvgItemCost_denormalized_10.yml",
        "src/workloads/execution/SecondaryReadsGenny.yml",
        "src/workloads/execution/TimeSeriesRangeDelete.yml",
        "src/workloads/execution/TimeSeriesArbitraryUpdate.yml",
        "src/workloads/execution/BackgroundValidateCmd.yml",
        "src/workloads/execution/PingCommand.yml",
        "src/workloads/execution/UserAcquisition.yml",
        "src/workloads/encrypted/medical_workload-guid-50-50-unencrypted.yml",
        "src/workloads/encrypted/medical_workload-diagnosis-100-0-unencrypted.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt1Cf16.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt1Cfdefault.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt5Cf32.yml",
        "src/workloads/encrypted/medical_workload-guid-50-50.yml",
        "src/workloads/encrypted/medical_workload-diagnosis-100-0.yml",
        "src/workloads/encrypted/ExponentialCompact.yml",
        "src/workloads/encrypted/medical_workload-diagnosis-50-50.yml",
        "src/workloads/encrypted/medical_workload-diagnosis-50-50-unencrypted.yml",
        "src/workloads/encrypted/medical_workload-load.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt5Cfdefault.yml",
        "src/workloads/encrypted/medical_workload-load-unencrypted.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt5Cf16.yml",
        "src/workloads/encrypted/YCSBLikeQueryableEncrypt1Cf32.yml",
        "src/workloads/query/locf.yml",
        "src/workloads/query/ExpressiveQueries.yml",
        "src/workloads/query/CollScanLargeNumberOfFieldsLarge.yml",
        "src/workloads/query/TimeSeriesSortOverlappingBuckets.yml",
        "src/workloads/query/AggregationsOutput.yml",
        "src/workloads/query/CumulativeWindowsMultiAccums.yml",
        "src/workloads/query/BooleanSimplifier.yml",
        "src/workloads/query/UnionWith.yml",
        "src/workloads/query/DensifyTimeseriesCollection.yml",
        "src/workloads/query/OneMDocCollection_LargeDocIntId.yml",
        "src/workloads/query/TenMDocCollection_ObjectId.yml",
        "src/workloads/query/CollScanPredicateSelectivityMedium.yml",
        "src/workloads/query/CollScanLargeNumberOfFieldsSmall.yml",
        "src/workloads/query/SetWindowFieldsUnbounded.yml",
        "src/workloads/query/ConstantFoldArithmetic.yml",
        "src/workloads/query/TimeSeries2dsphere.yml",
        "src/workloads/query/TimeSeriesLastpoint.yml",
        "src/workloads/query/CPUCycleMetricsInsert.yml",
        "src/workloads/query/SlidingWindowsMultiAccums.yml",
        "src/workloads/query/CollScanSimplifiablePredicateMedium.yml",
        "src/workloads/query/CPUCycleMetricsUpdate.yml",
        "src/workloads/query/TenMDocCollection_IntId_IdentityView.yml",
        "src/workloads/query/QueryStats.yml",
        "src/workloads/query/CollScanProjectionMedium.yml",
        "src/workloads/query/DensifyHours.yml",
        "src/workloads/query/DensifyMilliseconds.yml",
        "src/workloads/query/TenMDocCollection_IntId.yml",
        "src/workloads/query/UpdateLargeDocuments.yml",
        "src/workloads/query/LookupColocatedData.yml",
        "src/workloads/query/DensifyMonths.yml",
        "src/workloads/query/CPUCycleMetricsFind.yml",
        "src/workloads/query/DensifyNumeric.yml",
        "src/workloads/query/ExternalSort.yml",
        "src/workloads/query/MetricSecondaryIndexTimeseriesCollection.yml",
        "src/workloads/query/TenMDocCollection_SubDocId.yml",
        "src/workloads/query/linearFill.yml",
        "src/workloads/query/CollScanPredicateSelectivitySmall.yml",
        "src/workloads/query/TenMDocCollection_IntId_Agg.yml",
        "src/workloads/query/CollScanOnMixedDataTypesLarge.yml",
        "src/workloads/query/CollScanSimplifiablePredicateLarge.yml",
        "src/workloads/query/TenMDocCollection_ObjectId_Sharded.yml",
        "src/workloads/query/RepeatedPathTraversalSmall.yml",
        "src/workloads/query/RepeatedPathTraversalMedium.yml",
        "src/workloads/query/CollScanOnMixedDataTypesMedium.yml",
        "src/workloads/query/CollScanSimplifiablePredicateSmall.yml",
        "src/workloads/query/CumulativeWindows.yml",
        "src/workloads/query/CollScanOnMixedDataTypesSmall.yml",
        "src/workloads/query/BooleanSimplifierSmallDataset.yml",
        "src/workloads/query/CollScanPredicateSelectivityLarge.yml",
        "src/workloads/query/DensifyFillCombo.yml",
        "src/workloads/query/PipelineUpdate.yml",
        "src/workloads/query/CollScanLargeNumberOfFieldsMedium.yml",
        "src/workloads/query/TimeSeriesSortCompound.yml",
        "src/workloads/query/WindowWithNestedFieldProjection.yml",
        "src/workloads/query/FillTimeseriesCollection.yml",
        "src/workloads/query/GraphLookup.yml",
        "src/workloads/query/GraphLookupWithOnlyUnshardedColls.yml",
        "src/workloads/query/QueryStatsQueryShapes.yml",
        "src/workloads/query/CollScanComplexPredicateSmall.yml",
        "src/workloads/query/CollScanProjectionLarge.yml",
        "src/workloads/query/CollScanProjectionSmall.yml",
        "src/workloads/query/CollScanComplexPredicateLarge.yml",
        "src/workloads/query/TenMDocCollection_IntId_IdentityView_Agg.yml",
        "src/workloads/query/WindowWithComplexPartitionExpression.yml",
        "src/workloads/query/LookupWithOnlyUnshardedColls.yml",
        "src/workloads/query/Lookup.yml",
        "src/workloads/query/SlidingWindows.yml",
        "src/workloads/query/CPUCycleMetricsDelete.yml",
        "src/workloads/query/TimeSeriesSort.yml",
        "src/workloads/query/SortByExpression.yml",
        "src/workloads/query/CollScanComplexPredicateMedium.yml",
        "src/workloads/query/RepeatedPathTraversal.yml",
        "src/workloads/query/multiplanner/NoGoodPlan.yml",
        "src/workloads/query/multiplanner/Simple.yml",
        "src/workloads/query/multiplanner/ManyIndexSeeks.yml",
        "src/workloads/query/multiplanner/NoResults.yml",
        "src/workloads/query/multiplanner/NoSuchField.yml",
        "src/workloads/query/multiplanner/BlockingSort.yml",
        "src/workloads/query/multiplanner/MultiplannerWithGroup.yml",
        "src/workloads/query/multiplanner/ClusteredCollection.yml",
        "src/workloads/query/multiplanner/CompoundIndexes.yml",
        "src/workloads/query/multiplanner/UseClusteredIndex.yml",
        "src/workloads/query/multiplanner/NonBlockingVsBlocking.yml",
        "src/workloads/query/multiplanner/MultikeyIndexes.yml",
        "src/workloads/issues/MongosLatency.yml",
        "src/phases/HelloWorld/ExamplePhase2.yml",
        "src/phases/HelloWorld/ExamplePhase2.yml",
        "src/phases/scale/DesignDocWorkloadPhases.yml",
        "src/phases/scale/LargeScalePhases.yml",
        "src/phases/sharding/ShardCollectionTemplate.yml",
        "src/phases/sharding/SetClusterParameterTemplate.yml",
        "src/phases/tpch/normalized/Q7_normalized.yml",
        "src/phases/tpch/normalized/Q18_normalized.yml",
        "src/phases/tpch/normalized/Q19_normalized.yml",
        "src/phases/tpch/normalized/Q6_normalized.yml",
        "src/phases/tpch/normalized/Q4_normalized.yml",
        "src/phases/tpch/normalized/Q5_normalized.yml",
        "src/phases/tpch/normalized/Q1_normalized.yml",
        "src/phases/tpch/normalized/Q22_normalized.yml",
        "src/phases/tpch/normalized/Q2_normalized.yml",
        "src/phases/tpch/normalized/Q21_normalized.yml",
        "src/phases/tpch/normalized/Q20_normalized.yml",
        "src/phases/tpch/normalized/Q3_normalized.yml",
        "src/phases/tpch/normalized/Q11_normalized.yml",
        "src/phases/tpch/normalized/Q10_normalized.yml",
        "src/phases/tpch/normalized/Q12_normalized.yml",
        "src/phases/tpch/normalized/Q13_normalized.yml",
        "src/phases/tpch/normalized/Q8_normalized.yml",
        "src/phases/tpch/normalized/Q17_normalized.yml",
        "src/phases/tpch/normalized/Q16_normalized.yml",
        "src/phases/tpch/normalized/Q9_normalized.yml",
        "src/phases/tpch/normalized/Q14_normalized.yml",
        "src/phases/tpch/normalized/Q15_normalized.yml",
        "src/phases/tpch/denormalized/TotalLineitemRevenue_denormalized.yml",
        "src/phases/tpch/denormalized/Q7_denormalized.yml",
        "src/phases/tpch/denormalized/Q18_denormalized.yml",
        "src/phases/tpch/denormalized/Q19_denormalized.yml",
        "src/phases/tpch/denormalized/Q6_denormalized.yml",
        "src/phases/tpch/denormalized/AvgAcctBal_denormalized.yml",
        "src/phases/tpch/denormalized/Q4_denormalized.yml",
        "src/phases/tpch/denormalized/Q5_denormalized.yml",
        "src/phases/tpch/denormalized/Q1_denormalized.yml",
        "src/phases/tpch/denormalized/Q22_denormalized.yml",
        "src/phases/tpch/denormalized/BiggestOrders_denormalized.yml",
        "src/phases/tpch/denormalized/Q2_denormalized.yml",
        "src/phases/tpch/denormalized/Q21_denormalized.yml",
        "src/phases/tpch/denormalized/Q20_denormalized.yml",
        "src/phases/tpch/denormalized/Q3_denormalized.yml",
        "src/phases/tpch/denormalized/Q11_denormalized.yml",
        "src/phases/tpch/denormalized/Q10_denormalized.yml",
        "src/phases/tpch/denormalized/Q12_denormalized.yml",
        "src/phases/tpch/denormalized/Q13_denormalized.yml",
        "src/phases/tpch/denormalized/Q8_denormalized.yml",
        "src/phases/tpch/denormalized/TotalOrderRevenue_denormalized.yml",
        "src/phases/tpch/denormalized/Q17_denormalized.yml",
        "src/phases/tpch/denormalized/Q16_denormalized.yml",
        "src/phases/tpch/denormalized/Q9_denormalized.yml",
        "src/phases/tpch/denormalized/Q14_denormalized.yml",
        "src/phases/tpch/denormalized/Q15_denormalized.yml",
        "src/phases/tpch/denormalized/AvgItemCost_denormalized.yml",
        "src/phases/execution/ValidateCmd.yml",
        "src/phases/execution/CreateIndexPhase.yml",
        "src/phases/execution/TimeSeriesUpdatesAndDeletes.yml",
        "src/phases/execution/ClusteredCollection.yml",
        "src/phases/execution/config/MultiDeletes/Default.yml",
        "src/phases/execution/config/MultiDeletes/WithSecondaryIndexes.yml",
        "src/phases/encrypted/ContinuousWritesWithExponentialCompactTemplate.yml",
        "src/phases/encrypted/YCSBLikeEncryptedTemplate.yml",
        "src/phases/encrypted/YCSBLikeActorTemplate.yml",
        "src/phases/replication/startup/StartupPhasesTemplate.yml",
        "src/phases/query/CollScanSimplifiablePredicate.yml",
        "src/phases/query/BooleanSimplifier.yml",
        "src/phases/query/Multiplanner.yml",
        "src/phases/query/GroupStagesOnComputedFields.yml",
        "src/phases/query/TimeSeriesLastpoint.yml",
        "src/phases/query/CollScanOnMixedDataTypes.yml",
        "src/phases/query/IDHack.yml",
        "src/phases/query/RunLargeArithmeticOp.yml",
        "src/phases/query/CollScanProjection.yml",
        "src/phases/query/LookupCommands.yml",
        "src/phases/query/Views.yml",
        "src/phases/query/CollScanComplexPredicate.yml",
        "src/phases/query/CollScanComplexPredicateQueries.yml",
        "src/phases/query/AggregateExpressions.yml",
        "src/phases/query/TimeSeriesSortCommands.yml",
        "src/phases/query/GetBsonDate.yml",
        "src/phases/issues/ConnectionsBuildup.yml",
        "src/workloads/query/multiplanner/VariedSelectivity.yml",
        "src/workloads/query/multiplanner/Subplanning.yml",
    ]
)
