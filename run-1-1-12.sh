#!/usr/bin/bash

./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-1
./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-2
./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-3
./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-4
./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-5
./run-genny workload src/workloads/query/TimeseriesTsbsQuery-SingleGroupBy-1-1-12.yml
mv build/WorkloadOutput/CedarMetrics build/WorkloadOutput/$1-tsbs-query-single-groupby-t1-1-1-12-try-6
