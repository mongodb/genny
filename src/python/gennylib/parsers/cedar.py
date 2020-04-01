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
import logging
import csv
import sys
from collections import OrderedDict, defaultdict
from datetime import datetime
from os.path import join as pjoin

from bson import BSON
from bson.int64 import Int64

from gennylib.parsers.csv2 import CSV2, IntermediateCSVColumns
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
        # dict(operation -> dict(metric_column -> cumulative_value))
        self.cumulatives_for_op = defaultdict(lambda: defaultdict(int))

        # dict(operation -> total_duration)
        self.total_for_op = defaultdict(int)

        # dict(thread -> op -> prev_op_ts))
        self.prev_ts_of_op_thread = defaultdict(lambda: {})

    def __iter__(self):
        return self

    def __next__(self):
        return self._parse_into_cedar_format(next(self.raw_reader))

    def _compute_cumulatives(self, line):
        op = line[IntermediateCSVColumns.OPERATION]

        n = IntermediateCSVColumns.N
        ops = IntermediateCSVColumns.OPS
        size = IntermediateCSVColumns.SIZE
        err = IntermediateCSVColumns.ERRORS
        dur = IntermediateCSVColumns.DURATION
        for col in [n, ops, size, err, dur]:
            self.cumulatives_for_op[op][col] += line[col]

        ts_col = IntermediateCSVColumns.TS
        thread = line[IntermediateCSVColumns.THREAD]

        # total_duration is duration for the first operation on each thread
        # because we don't track when each thread starts.
        if thread in self.prev_ts_of_op_thread and op in self.prev_ts_of_op_thread[thread]:
            cur_total = line[ts_col] - self.prev_ts_of_op_thread[thread][op]
        else:
            cur_total = line[dur]

        self.total_for_op[op] += cur_total
        self.prev_ts_of_op_thread[thread][op] = line[ts_col]

    def _parse_into_cedar_format(self, line):
        # Compute all cumulative values for simplicity; Not all values are used.
        self._compute_cumulatives(line)

        # milliseconds to seconds to datetime.datetime()
        ts = datetime.utcfromtimestamp(line[IntermediateCSVColumns.UNIX_TIME] / 1000)
        op = line[IntermediateCSVColumns.OPERATION]

        res = OrderedDict(
            [
                ("ts", ts),
                ("id", Int64(line[IntermediateCSVColumns.THREAD])),
                (
                    "counters",
                    OrderedDict(
                        [
                            ("n", Int64(self.cumulatives_for_op[op][IntermediateCSVColumns.N])),
                            ("ops", Int64(self.cumulatives_for_op[op][IntermediateCSVColumns.OPS])),
                            (
                                "size",
                                Int64(self.cumulatives_for_op[op][IntermediateCSVColumns.SIZE]),
                            ),
                            (
                                "errors",
                                Int64(self.cumulatives_for_op[op][IntermediateCSVColumns.ERRORS]),
                            ),
                        ]
                    ),
                ),
                (
                    "timers",
                    OrderedDict(
                        [
                            (
                                "duration",
                                Int64(self.cumulatives_for_op[op][IntermediateCSVColumns.DURATION]),
                            ),
                            ("total", Int64(self.total_for_op[op])),
                        ]
                    ),
                ),
                ("gauges", OrderedDict([("workers", Int64(line[IntermediateCSVColumns.WORKERS]))])),
            ]
        )

        return res, op


def split_into_actor_operation_and_transform_to_cedar_format(file_name, out_dir):
    """
    Split up the IntermediateCSV format input into one or more BSON files in Cedar
    format; one file for each (actor, operation) pair.

    :return: a list of cedar metrics file names
    """

    # remove the ".csv" suffix to get the actor name.
    actor_name = file_name[:-4]

    out_files = {}
    with open(pjoin(out_dir, file_name)) as in_f:
        reader = csv.reader(in_f, quoting=csv.QUOTE_NONNUMERIC)
        next(reader)  # Ignore the header row.
        try:
            for ordered_dict, op in IntermediateCSVReader(reader):
                out_file_name = pjoin(out_dir, "{}-{}.bson".format(actor_name, op))
                if out_file_name not in out_files:
                    out_files[out_file_name] = open(out_file_name, "wb")
                out_files[out_file_name].write(BSON.encode(ordered_dict))
        finally:
            for fh in out_files.values():
                fh.close()

    return out_files.keys()


def split_into_actor_csv_files(data_reader, out_dir):
    """
    Split up the monolithic genny metrics csv2 file into smaller [actor].csv files
    """
    cur_out_csv = None
    cur_out_fh = None
    cur_actor = (None, None)
    output_files = []

    # Print progress to prevent the CI task from timing out.
    counter = 0

    for line, actor in data_reader:
        if actor != cur_actor:
            cur_actor = actor

            # Close out old file.
            if cur_out_fh:
                cur_out_fh.close()

            # Open new csv file.
            file_name = actor + ".csv"
            output_files.append(file_name)
            cur_out_fh = open(pjoin(out_dir, file_name), "w", newline="")

            # Quote non-numeric values so they get converted to float automatically
            cur_out_csv = csv.writer(cur_out_fh, quoting=csv.QUOTE_NONNUMERIC)
            cur_out_csv.writerow(IntermediateCSVColumns.default_columns())

        cur_out_csv.writerow(line)

        counter += 1
        if counter % 1e6 == 0:
            print("Parsed {} metrics".format(counter))

    if cur_out_fh is not None:
        cur_out_fh.close()

    return output_files


def sort_csv_file(file_name, out_dir):
    # Sort on Timestamp and Thread.
    csvsort(
        pjoin(out_dir, file_name),
        [IntermediateCSVColumns.TS, IntermediateCSVColumns.THREAD],
        quoting=csv.QUOTE_NONNUMERIC,
        has_header=True,
        show_progress=True,
    )


def build_parser():
    parser = argparse.ArgumentParser(
        description="Convert Genny csv2 perf data to Cedar BSON format"
    )
    parser.add_argument("input_file", metavar="input-file", help="path to genny csv2 perf data")
    parser.add_argument(
        "output_dir", metavar="output-dir", help="directory to store output BSON files"
    )
    return parser


def run(args):
    """
    Runs the conversion from genny metrics to cedar format.

    :param args: parsed command line args.
    :return: list of cedar metrics file names and the approximate run time of the test
             computed using the machine's system_time.

    """
    out_dir = args.output_dir

    # Read CSV2 file
    my_csv2 = CSV2(args.input_file)
    metrics_file_names = []

    with my_csv2.data_reader() as data_reader:
        # Separate into actor-operation
        files = split_into_actor_csv_files(data_reader, out_dir)

        for f in files:
            logging.info("Processing metrics in file %s", f)
            # csvsort by timestamp, thread
            sort_csv_file(f, out_dir)

            # compute cumulative and stream output to bson file
            metrics_file_names.extend(
                split_into_actor_operation_and_transform_to_cedar_format(f, out_dir)
            )

    return metrics_file_names, my_csv2.approximate_test_run_time


def main__cedar(argv=sys.argv[1:]):
    parser = build_parser()
    args = parser.parse_args(argv)
    run(args)
