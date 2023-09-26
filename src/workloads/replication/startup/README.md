## BACKGROUND

This directory contains some Genny workloads to test the performance of the mongod shutdown/startup process.

## TEST DESCRIPTION

The test is structured with several Genny workloads that are executed in order by a DSI custom test control ([test_control.startup.yml](https://github.com/10gen/dsi/blob/a95664a27e39d7f1eebec6e685d646b8e54a334c/configurations/test_control/test_control.startup.yml)) that restarts the mongod between the workloads, allowing us to measure shutdown/startup performance of each phase.

To ensure DSI restarts mongod after the last workload to measure its shutdown/startup performance, a [dummy read workload](https://github.com/10gen/dsi/blob/a95664a27e39d7f1eebec6e685d646b8e54a334c/configurations/test_control/test_control.startup.yml#L67-L72) has been added as the final workload.

### The test has two phases:
  
  **1. Light load:**

    - Workload names preceded with "1_".
    - Collection count: 5.
    - Document count: 100K.
    - Approx document size: 10KB.
    - Data size per collection: 1GB.
    - Total data size: 5GB.
    - Number of secondary indexes per collection: 10.
    - Number of storage tables: 55+.

  **2. Heavy load**:

    - Workload names preceded with "2_".
    - Collection count: 10K.
    - Document count: 5K.
    - Approx document size: 1KB.
    - Data size per collection: 5MB.
    - Total data size: 50GB.
    - Number of secondary indexes per collection: 10.
    - Number of storage tables: 100k+.
    
### Workloads for each phase:

  DSI custom test control ([test_control.startup.yml](https://github.com/10gen/dsi/blob/a95664a27e39d7f1eebec6e685d646b8e54a334c/configurations/test_control/test_control.startup.yml)) assigns a descriptive run ID to each workload, as illustrated below.

  | Phase | Light load | Heavy load |
  | --------| -------- | -------- |
  | 1. Load the data.<br>Loading the initial load mentioned above. | [Step_1_0_load_5GB_55_storage_tables](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/1_0_5GB.yml) | [Step_2_0_load_50GB_100k_storage_tables](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/2_0_50GB.yml) |
  | 2. Stop checkpointing and add update CRUD operations.<br>(which controls the number of oplog entries that will be replayed in startup recovery). | [Step_1_1_5GB_crud_50k_ops](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/1_1_5GB_crud.yml) | [Step_2_1_50GB_crud_10M_ops](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/2_1_50GB_crud.yml) |
  | 3. Stop checkpointing and add DDL and CRUD operations.<br> It establishes a new database, generates new collections, builds secondary indexes, and proceeds to insert data. | [Step_1_2_5GB_ddl_55_storage_tables](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/1_2_5GB_ddl.yml) <br>- Creates **1 new database**. <br>- Creates **5 collections** with extra **10 indexes**. <br>- Inserts documents for an approximate total of **~1MB per collection**, **~5MB total**. | [Step_2_2_50GB_ddl_10k_storage_tables](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/2_2_50GB_ddl.yml) <br>- Creates **1 new database**. <br>- Creates **1000 collections** with extra **10 indexes**.<br>- Inserts documents for an approximate total of **100KB per collection**, **~1GB total**. |
  | 4. Stop any IndexBuild from getting completed and start an IndexBuild that will continue after startup recovery.<br>(It drops all the 10 secondary indexes built in the first step and starts building them again which will get paused). | [Step_1_3_5GB_index](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/1_3_5GB_index.yml) | [Step_2_3_50GB_index](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/3_0_Reads.yml) |

To ensure DSI restarts mongod after the last workload, a dummy read workload has been added as the final workload.

## Reading Results

The time taken for startup recovery and shutdown is tracked in these two timing metrics: 
| Metric | Description |
| ---------- | ------------ |  
| ShutdownTime | The time in milliseconds it takes to shutdown all launched mongod programs.	|
| RecoveryTimeAfterRestart | The "recovery" time in milliseconds that it takes to start the server after this particular test phase / session ends. This metric is equal to the start time of the subsequent phase. |

**Notes**
- The startup task is currently running on a single node replica set variant, so these metrics are giving us the time for that single node to shutdown/startup.
- Metrics for the dummy last phase should be ignored (starts with Step_3_0).

**Example**

  | Test | Measurement | Patch Result |
  | -------- | -------- | -------- |
  | Step_1_1_5GB_crud_50k_ops Timing Metrics | ShutdownTime0 | 2605.681189 |
  | Step_1_1_5GB_crud_50k_ops Timing Metrics | RecoveryTimeAfterRestart0 | 98552.713478 |
  | Step_2_1_50GB_crud_10M_ops Timing Metrics | ShutdownTime0 | 11425.647337 |
  | Step_2_1_50GB_crud_10M_ops Timing Metrics | RecoveryTimeAfterRestart0 | 960575.04176 |
  

That shows that after running [Step_1_1_5GB_crud_50k_ops](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/1_1_5GB_crud.yml) workload the mongod server took **2.6 sec** to shutdown then **98.5 sec** during startup recovery, while [Step_2_1_50GB_crud_10M_ops](https://github.com/mongodb/genny/blob/1189cda0aa80e6104c7fe87016f1a3de98b18dba/src/workloads/replication/startup/2_1_50GB_crud.yml) took around **11 sec** and **16 min** respectively.