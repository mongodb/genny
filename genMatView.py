#!/usr/bin/python
from string import Template

# Open a file: file
file = open('./src/workloads/transactions/templates/MatView.tmpl',mode='r')
# read all lines at once
temp_str = file.read()
# close the file
file.close()

temp_obj = Template(temp_str)

isTransactionalOpts = ['true', 'false']
numInitialDocsOpts = ['100000']
minBaseDocSizeBytesOpts = ['1000']
numGroupsAndDistributionOpts = [
    '{distribution: uniform, min: 1, max: 1}',
    '{distribution: uniform, min: 1, max: 10}',
    '{distribution: uniform, min: 1, max: 100}',
    '{distribution: uniform, min: 1, max: 1000}',
    '{distribution: uniform, min: 1, max: 10000}',
    '{distribution: binomial, t: 100, p: 0.05}',
    '{distribution: geometric, p: 0.1}',
]
numClientThreadsOpts = ['1', '2', '4', '8', '16', '32']
numClientBatchesOpts = ['100']
numInsertsPerClientBatchOpts = ['100']
insertModeOpts = ['insertMany', 'insertOne']
numMatViewsOpts= ['0', '1', '2', '4', '8']
matViewModeOpts = ['incremental']

name_temp_str='MatView_${isTransactional}_${numClientThreads}thread_${numClientBatches}batches_${numInsertsPerClientBatch}perbatch_${insertMode}_${numMatViews}views_${matViewMode}_${numGroupsAndDistribution}.yml'
name_temp_obj = Template(name_temp_str)

for isTransactional in isTransactionalOpts:
    for numInitialDocs in numInitialDocsOpts:
        for minBaseDocSizeBytes in minBaseDocSizeBytesOpts:
            for numGroupsAndDistribution in numGroupsAndDistributionOpts:
                for numClientThreads in numClientThreadsOpts:
                    for numClientBatches in numClientBatchesOpts:
                        for numInsertsPerClientBatch in numInsertsPerClientBatchOpts:
                            for insertMode in insertModeOpts:
                                for numMatViews in numMatViewsOpts:
                                    for matViewMode in matViewModeOpts:
                                        res = temp_obj.substitute(
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
                                        )
                                        name=name_temp_obj.substitute(
                                            isTransactional = "transactional" if isTransactional else "nonxact",
                                            # numInitialDocs = numInitialDocs,
                                            # minBaseDocSizeBytes = minBaseDocSizeBytes,
                                            numGroupsAndDistribution = numGroupsAndDistribution.replace(" ", "").replace("{", "_").replace("}", "_").replace(":", "_").replace(",", "_").replace(".", "_"),
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
