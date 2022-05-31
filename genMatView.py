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

isShardedOpts = [True, False]

numClientThreadsOpts = ['1', '2', '4', '8', '16', '32']

name_temp_str='MatView_${sharded}_${numClientThreads}client.yml'
name_temp_obj = Template(name_temp_str)

for isSharded in isShardedOpts:
    for numClientThreads in numClientThreadsOpts:
        res = temp_obj.substitute(
            __matViewParamsFile__ = 'MatViewParamsSharded.yml' if isSharded else 'MatViewParamsReplicated.yml',
            __numClientThreads__ = numClientThreads,
            __autoRunConf__ = shardedAutoRun if isSharded else nonshardedAutoRun,
        )
        name=name_temp_obj.substitute(
            sharded = "sharded" if isSharded else "replicated",
            numClientThreads = numClientThreads,
        )

        text_file = open("./src/workloads/scale/"+name, "w")
        n = text_file.write(res)
        text_file.close()
