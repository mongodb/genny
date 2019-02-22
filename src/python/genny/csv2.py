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

"""
Thin object wrapper around a genny metrics' "csv2" file with metadata stored
as native Python variables.
"""
import csv


class _CSV2Dialect(csv.unix_dialect):
    strict = True
    skipinitialspace = True
    quoting = csv.QUOTE_MINIMAL  # Expect no quotes
    # We don't allow commas in actor names at the moment, so no need
    # to escape the delimiter.
    # escapechar = '\\'


class CSV2:

    def __init__(self, csv2_file_name):
        # parsers for newline-delimited header sections at the top of each
        # csv2 file.
        header_parsers = [
            self.parse_clocks,
            self.parse_thread_count,
            self.parse_operations
        ]

        # The number to add to the timestamp (a.k.a. system_time) to get
        # the UNIX time.
        self._unix_epoch_offset_ns = None

        # map of (actor, operation) to thread count
        self._operation_thread_count_map = {}

        with open(csv2_file_name, 'r')  as f:
            reader = csv.reader(f, dialect=_CSV2Dialect)
            for parser in header_parsers:
                parser(reader)

    def parse_clocks(self, reader):
        title = next(reader)
        if not title:
            raise ValueError('Expected title line, got: %s', title)

        if title[0] != 'Clocks':
            raise ValueError('Expected tile to be "Clocks", got %s', title[0])




    def parse_thread_count(self, reader):
        pass

    def parse_operations(self, reader):
        pass
