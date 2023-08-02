## BACKGROUND

This directory contains some Genny workloads to test the performance of the mongod shutdown/startup process.

## TEST DESCRIPTION

The test is structured with several Genny workloads that are executed in order by a DSI custom test control that restarts the mongod between the workloads, allowing us to measure shutdown/startup performance.

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

  1. Load the data.
  2. Stop checkpointing and add update CRUD operations (which controls the number of oplogs that will be replayed in startup recovery).
  3. Stop checkpointing and add DDL and CRUD operations.
  4. Stop any IndexBuild from getting completed and start an IndexBuild that will continue after startup recovery.

To ensure DSI restarts mongod after the last workload, a dummy read workload has been added as the final workload.
