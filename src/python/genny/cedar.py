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
from collections import OrderedDict
from os.path import join as pjoin

from bson import BSON

from genny.csv2 import CSV2, IntermediateCSVColumns
from third_party.csvsort import csvsort

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


class IntermediateCSVReader:
    """
    Class that reads IntermediateCSVColumns and outputs Cedar BSON
    """

    def __init__(self, reader):
        self.raw_reader = reader
        self.cumulatives = [0 for _ in range(len(IntermediateCSVColumns.default_columns()))]

        # Create some variables to help compute the cumulative CPU time; as it isn't
        # explicitly listed in the intermediate CSV.
        self.cumulative_cpu_time = 0
        self.prev_ts_by_thread = {}

    def __iter__(self):
        return self

    def __next__(self):
        return self._parse_into_cedar_format(next(self.raw_reader))

    def _parse_into_cedar_format(self, line):
        # Compute all cumulative values for simplicity; Not all values are used.
        self.cumulatives = [sum(v) for v in zip(line, self.cumulatives)]

        thread = line[IntermediateCSVColumns.THREAD]
        ts = line[IntermediateCSVColumns.SYSTEM_TS]

        # Compute the CPU time for the current operation on the current thread
        # and add it to the cumulative CPU time.
        if thread not in self.prev_ts_by_thread:
            self.prev_ts_by_thread[thread] = ts
        cur_op_cpu_time = ts - self.prev_ts_by_thread[thread]
        self.cumulative_cpu_time += cur_op_cpu_time
        self.prev_ts_by_thread[thread] = ts

        res = OrderedDict([
            ('ts', line[IntermediateCSVColumns.UNIX_TIME]),
            ('id', thread),
            ('counters', OrderedDict([
                ('n', self.cumulatives[IntermediateCSVColumns.N]),
                ('ops', self.cumulatives[IntermediateCSVColumns.OPS]),
                ('size', self.cumulatives[IntermediateCSVColumns.SIZE]),
                ('errors', self.cumulatives[IntermediateCSVColumns.ERRORS])
            ])),
            ('timers', OrderedDict([
                ('duration', self.cumulatives[IntermediateCSVColumns.DURATION]),
                ('total', self.cumulative_cpu_time)
            ])),
            ('gauges', OrderedDict([
                ('workers', line[IntermediateCSVColumns.WORKERS])
            ]))
        ])

        return res


def compute_cumulative_and_write_to_bson(file_name, out_dir):
    # Remove ".csv" and add ".bson"
    out_file_name = file_name[:-4] + '.bson'
    with open(pjoin(out_dir, out_file_name), 'wb') as out_f, open(
            pjoin(out_dir, file_name)) as in_f:
        reader = csv.reader(in_f, quoting=csv.QUOTE_NONNUMERIC)
        next(reader)  # Ignore the header row.
        for ordered_dict in IntermediateCSVReader(reader):
            out_f.write(BSON.encode(ordered_dict))


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
            cur_out_csv.writerow(IntermediateCSVColumns.default_columns())

        cur_out_csv.writerow(line)

    cur_out_fh.close()

    return output_files


def sort_csv_file(file_name, out_dir):
    # Sort on Timestamp and Thread.
    csvsort(pjoin(out_dir, file_name),
            [IntermediateCSVColumns.UNIX_TIME, IntermediateCSVColumns.THREAD],
            quoting=csv.QUOTE_NONNUMERIC,
            has_header=True,
            show_progress=True)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description='Convert Genny csv2 perf data to Cedar BSON format',
    )
    parser.add_argument('input_file', metavar='input-file', help='path to genny csv2 perf data')
    parser.add_argument('output_dir', metavar='output-dir',
                        help='directory to store output BSON files')

    return parser.parse_args(argv)


def main__cedar(argv=sys.argv[1:]):
    args = parse_args(argv)
    out_dir = args.output_dir

    # Read CSV2 file
    my_csv2 = CSV2(args.input_file)

    with my_csv2.data_reader() as data_reader:
        # Separate into actor-operation
        files = split_into_actor_operation_csv_files(data_reader, out_dir)

        for f in files:
            # csvsort by timestamp, thread
            sort_csv_file(f, out_dir)

            # compute cumulative and stream output to bson file
            compute_cumulative_and_write_to_bson(f, out_dir)
