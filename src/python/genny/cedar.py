#!/usr/bin/env python3
#
# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import csv
import sys
from os.path import join as pjoin

from csvsort import csvsort

from genny.csv2 import CSV2

"""
Convert raw genny csv output to a format expected by Cedar
(a.k.a. MongoDB Evergreen Expanded Metrics)

Sample input:
```
Clocks
SystemTime,100000000000000000
MetricsTime,86491166632088

OperationThreadCounts
InsertRemove,Insert,5

Operations
timestamp     , actor       , thread, operation, duration, outcome, n, ops, errors, size
86491166632088, InsertRemove, 0     , Insert   , 100     , 0      , 1, 6  , 2     , 40
86491166632403, InsertRemove, 0     , Insert   , 310     , 0      , 1, 9  , 3     , 60
86491166632661, InsertRemove, 0     , Insert   , 180     , 0      , 1, 8  , 6     , 20
86491166632088, InsertRemove, 1     , Insert   , 280     , 1      , 1, 3  , 1     , 20
86491166632316, InsertRemove, 1     , Insert   , 190     , 1      , 1, 4  , 1     , 30
86491166632088, InsertRemove, 2     , Insert   , 30      , 0      , 1, 4  , 1     , 30
86491166632245, InsertRemove, 2     , Insert   , 80      , 1      , 1, 8  , 7     , 10
86491166632088, InsertRemove, 3     , Insert   , 110     , 0      , 1, 7  , 2     , 50
86491166632088, InsertRemove, 4     , Insert   , 40      , 0      , 1, 9  , 0     , 90
```

Sample output:
```
{ts:TS(0),id:0,counters:{n:1,ops:6,size:40,errors:2},timers:{duration:100,total:100},gauges:{workers:5}}
{ts:TS(0),id:1,counters:{n:2,ops:9,size:60,errors:3},timers:{duration:380,total:380},gauges:{workers:5}}
{ts:TS(0),id:2,counters:{n:3,ops:13,size:90,errors:4},timers:{duration:410,total:410},gauges:{workers:5}}
{ts:TS(0),id:3,counters:{n:4,ops:20,size:140,errors:6},timers:{duration:520,total:520},gauges:{workers:5}}
{ts:TS(0),id:4,counters:{n:5,ops:29,size:230,errors:6},timers:{duration:560,total:560},gauges:{workers:5}}
{ts:TS(157),id:2,counters:{n:6,ops:37,size:240,errors:13},timers:{duration:640,total:717},gauges:{workers:5}}
{ts:TS(228),id:1,counters:{n:7,ops:41,size:270,errors:14},timers:{duration:830,total:945},gauges:{workers:5}}
{ts:TS(315),id:0,counters:{n:8,ops:50,size:330,errors:17},timers:{duration:1140,total:1260},gauges:{workers:5}}
{ts:TS(573),id:0,counters:{n:9,ops:58,size:350,errors:23},timers:{duration:1320,total:1518},gauges:{workers:5}}
```
"""


def split_into_actor_operation_csv_files(data_reader, out_dir):
    """
    Split up the monolithic genny metrics csv2 file into smaller [actor]-[operation].csv files
    """
    cur_out_csv = None
    cur_out_fh = None
    cur_actor_op_pair = (None, None)
    output_files = []

    for line, actor, op in data_reader:
        if (actor, op) != cur_actor_op_pair:
            cur_actor_op_pair = (actor, op)

            # Close out old file.
            if cur_out_fh:
                cur_out_fh.close()

            # Open new csv file.
            file_name = actor + '-' + op + '.csv'
            output_files.append(file_name)
            cur_out_fh = open(pjoin(out_dir, file_name), 'w', newline='')

            # Quote non-numeric values so they get converted to float automatically
            cur_out_csv = csv.writer(cur_out_fh, quoting=csv.QUOTE_NONNUMERIC)

        cur_out_csv.writerow(line)

    cur_out_fh.close()

    return output_files


def sort_csv_files(files, out_dir):
    for f in files:
        # Sort on Timestamp and Thread.
        csvsort(pjoin(out_dir, f), [0, 1], quoting=csv.QUOTE_NONNUMERIC, has_header=False,
                show_progress=True)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description='Convert Genny csv2 perf data to Cedar BSON format',
    )
    parser.add_argument('input_file', metavar='input-file', help='path to genny csv2 perf data')
    parser.add_argument('output_dir', metavar='output_dir',
                        help='directory to store output BSON files')

    return parser.parse_args(argv)


def main__cedar(argv=sys.argv[1:]):
    args = parse_args(argv)

    # Read CSV2 file
    my_csv2 = CSV2(args.input_file)

    # Separate into actor-operation
    files = split_into_actor_operation_csv_files(my_csv2.data_reader(), args.output_dir)

    # csvsort by timestamp, thread
    sort_csv_files(files, args.output_dir)

    # stream output to bson file
