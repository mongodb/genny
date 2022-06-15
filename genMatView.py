#!/usr/bin/python
from string import Template

# Open a file: file
file = open('./src/workloads/transactions/templates/MatView.tmpl',mode='r')
# read all lines at once
temp_str = file.read()
# close the file
file.close()

temp_obj = Template(temp_str)

nonshardedAutoRun = """
- When:
    mongodb_setup:
      $eq:
      - replica
      - single-replica
"""

shardedAutoRun = """
- When:
    mongodb_setup:
      $eq:
      - shard
      - shard-lite
"""

def renameDist(d):
    return d.replace(" ", "").replace("{", "_").replace("}", "_").replace(":", "_").replace(",", "_").replace(".", "_")

experimentTypeOpts = ["wild-card-index-exp", "mat-view-exp"]
isShardedOpts = [
    True,
    False
]
isTransactionalOpts = [True, False]
numInitialDocsOpts = ['10000']
minBaseDocSizeBytesOpts = ['1000']
numGroupsAndDistributionOpts = {
    'singlekey': '{distribution: uniform, min: 1, max: 1}',
    # 'uniform10':'{distribution: uniform, min: 1, max: 10}',
    'uniform100':'{distribution: uniform, min: 1, max: 100}',
    # 'uniform1000':'{distribution: uniform, min: 1, max: 1000}',
    'uniform10000':'{distribution: uniform, min: 1, max: 10000}',
    # 'binomial':'{distribution: binomial, t: 100, p: 0.05}',
    # 'geometric':'{distribution: geometric, p: 0.1}',
}
numClientThreadsOpts = [
    '1',
    # '2',
    '4',
    # '8',
    '16',
    # '32',
]
numClientBatchesOpts = ['100']
numInsertsPerClientBatchOpts = ['100']
insertModeOpts = ['insertMany', 'insertOne']
numMatViewsOpts= [
    '0',
    '1',
    '2',
    # '4',
    # '8',
]
matViewModeOpts = [
    # 'sync-incremental',
    # 'async-incremental-result-delta',
    # 'async-incremental-base-delta',
    'async-incremental-result-delta-not-colocated',
    # 'full-refresh'
]


name_temp_str='MV_${experimentType}_${sharded}_${isTransactional}_${numClientThreads}thread_${numClientBatches}batches_${numInsertsPerClientBatch}perbatch_${insertMode}_${numMatViews}views_${matViewMode}_${numGroupsAndDistribution}.yml'
name_temp_obj = Template(name_temp_str)

def createExperiment(
    experimentType,
    isSharded,
    isTransactional,
    numInitialDocs,
    minBaseDocSizeBytes,
    numGroupsAndDistributionKey,
    numGroupsAndDistribution,
    numClientThreads,
    numClientBatches,
    numInsertsPerClientBatch,
    insertMode,
    numMatViews,
    matViewMode,
):
    res = temp_obj.substitute(
        __matViewParamsFile__ = ('WildCardIndexParamsSharded.yml' if isSharded else 'WildCardIndexParamsReplicated.yml') if experimentType == 'wild-card-index-exp' else  ('MatViewParamsSharded.yml' if isSharded else 'MatViewParamsReplicated.yml'),
        __isTransactional__ = isTransactional,
        __numInitialDocs__ = numInitialDocs,
        __minBaseDocSizeBytes__ = minBaseDocSizeBytes,
        __numGroupsAndDistribution__ = numGroupsAndDistribution,
        __numClientThreads__ = numClientThreads,
        __numClientBatches__ = numClientBatches,
        __numInsertsPerClientBatch__ = numInsertsPerClientBatch,
        __insertMode__ = insertMode,
        __numMatViews__ = numMatViews,
        __matViewMode__ = matViewMode,
        __autoRunConf__ = shardedAutoRun if isSharded else nonshardedAutoRun,
    )
    name=name_temp_obj.substitute(
        experimentType = experimentType,
        sharded = "sharded" if isSharded else "replset",
        isTransactional = "transactional" if isTransactional else "nonxact",
        # numInitialDocs = numInitialDocs,
        # minBaseDocSizeBytes = minBaseDocSizeBytes,
        numGroupsAndDistribution = numGroupsAndDistributionKey,
        numClientThreads = numClientThreads,
        numClientBatches = numClientBatches,
        numInsertsPerClientBatch = numInsertsPerClientBatch,
        insertMode = insertMode,
        numMatViews = numMatViews,
        matViewMode = matViewMode,
    )

    text_file = open("./src/workloads/scale/"+name, "w")
    n = text_file.write(res)
    text_file.close()


for isSharded in isShardedOpts:
    for isTransactional in isTransactionalOpts:
        for numInitialDocs in numInitialDocsOpts:
            for minBaseDocSizeBytes in minBaseDocSizeBytesOpts:
                for numClientThreads in numClientThreadsOpts:
                    for numClientBatches in numClientBatchesOpts:
                        for numInsertsPerClientBatch in numInsertsPerClientBatchOpts:
                            for insertMode in insertModeOpts:
                                for experimentType in experimentTypeOpts:
                                    if experimentType == 'wild-card-index-exp':
                                        createExperiment(
                                            experimentType,
                                            isSharded,
                                            isTransactional,
                                            numInitialDocs,
                                            minBaseDocSizeBytes,
                                            'singlekey',
                                            numGroupsAndDistributionOpts['singlekey'],
                                            numClientThreads,
                                            numClientBatches,
                                            numInsertsPerClientBatch,
                                            insertMode,
                                            0,
                                            'wild-card-index',
                                        )
                                    else:
                                        for numMatViews in numMatViewsOpts:
                                            if numMatViews > 0:
                                                for matViewMode in matViewModeOpts:
                                                    if matViewMode == 'async-incremental-base-delta':
                                                        createExperiment(
                                                            experimentType,
                                                            isSharded,
                                                            isTransactional,
                                                            numInitialDocs,
                                                            minBaseDocSizeBytes,
                                                            'singlekey',
                                                            numGroupsAndDistributionOpts['singlekey'],
                                                            numClientThreads,
                                                            numClientBatches,
                                                            numInsertsPerClientBatch,
                                                            insertMode,
                                                            10,
                                                            matViewMode,
                                                        )
                                                        break
                                                    else:
                                                        for numGroupsAndDistributionKey, numGroupsAndDistribution in numGroupsAndDistributionOpts.items():
                                                            createExperiment(
                                                                experimentType,
                                                                isSharded,
                                                                isTransactional,
                                                                numInitialDocs,
                                                                minBaseDocSizeBytes,
                                                                numGroupsAndDistributionKey,
                                                                numGroupsAndDistribution,
                                                                numClientThreads,
                                                                numClientBatches,
                                                                numInsertsPerClientBatch,
                                                                insertMode,
                                                                numMatViews,
                                                                matViewMode,
                                                            )
                                            else:
                                                createExperiment(
                                                    experimentType,
                                                    isSharded,
                                                    isTransactional,
                                                    numInitialDocs,
                                                    minBaseDocSizeBytes,
                                                    'singlekey',
                                                    numGroupsAndDistributionOpts['singlekey'],
                                                    numClientThreads,
                                                    numClientBatches,
                                                    numInsertsPerClientBatch,
                                                    insertMode,
                                                    0,
                                                    'none',
                                                )

