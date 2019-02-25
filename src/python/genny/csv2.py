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


import csv


class _Dialect(csv.unix_dialect):
    strict = True
    skipinitialspace = True
    quoting = csv.QUOTE_MINIMAL  # Expect no quotes in csv2.
    # We don't allow commas in actor names at the moment, so no need
    # to escape the delimiter.
    # escapechar = '\\'


class CSV2ParsingError(BaseException):
    pass


class CSVColumns(object):
    """
    Object oriented access to csv header/column mapping.
    """
    _COLUMNS = None

    @classmethod
    def add_columns(cls, columns):
        for i in range(len(columns)):
            cls.add_column(columns[i], i)

    @classmethod
    def add_column(cls, col_name, col_index):
        upper_col_name = col_name.upper()  # Python class constants should be uppercase.

        if not hasattr(cls, upper_col_name):
            raise CSV2ParsingError('%s doesn\'t have column %s', cls.__name__, col_name)

        if not isinstance(cls._COLUMNS, set):
            raise ValueError('Subclass must have the class property `_COLUMN = set()`')

        setattr(cls, upper_col_name, col_index)
        cls._COLUMNS.add(upper_col_name)

    @classmethod
    def validate(cls, expected_cols_set):
        val = cls._COLUMNS == expected_cols_set
        return val


class _OpListReader:
    """
    Thin wrapper around csv.DictReader() that eagerly reads the list of operations
    from csv2 and converts any digits to native Python integers and massages
    output into IntermediateCSV format.
    """

    def __init__(self, csv_reader_at_op, thread_count_map, ts_offset):
        """
        :param csv_reader_at_op: A CSV reader with its cursor on the first operation line.
        :param thread_count_map:
        :param ts_offset:
        """
        self.raw_reader = csv_reader_at_op
        self.tc_map = thread_count_map
        self.ts_offset = ts_offset

    def __iter__(self):
        return self

    def __next__(self):
        return self._parse_into_intermediate_csv(next(self.raw_reader))

    def _parse_into_intermediate_csv(self, line):
        for i in range(len(line)):

            # Strip spaces from lines
            line[i] = line[i].strip()

            # Convert string digits to ints
            if line[i].isdigit():
                line[i] = int(line[i])

        # Eagerly error if OUTCOME is > 1
        if line[_OpColumns.OUTCOME] > 1:
            raise CSV2ParsingError('Unexpected outcome on line %d: %s', self.raw_reader.line_num,
                                   line)

        # Convert nanoseconds to milliseconds and add offset
        line[_OpColumns.TIMESTAMP] /= 1000 * 1000
        line[_OpColumns.TIMESTAMP] += self.ts_offset

        op = line[_OpColumns.OPERATION]
        actor = line[_OpColumns.ACTOR]

        # Transform output into IntermediateCSV format.
        out = [None for _ in range(len(IntermediateCSVColumns.default_columns()))]
        out[IntermediateCSVColumns.TS_MS] = line[_OpColumns.TIMESTAMP]
        out[IntermediateCSVColumns.THREAD] = line[_OpColumns.THREAD]
        out[IntermediateCSVColumns.DURATION] = line[_OpColumns.DURATION]
        out[IntermediateCSVColumns.OUTCOME] = line[_OpColumns.OUTCOME]
        out[IntermediateCSVColumns.N] = line[_OpColumns.N]
        out[IntermediateCSVColumns.OPS] = line[_OpColumns.OPS]
        out[IntermediateCSVColumns.ERRORS] = line[_OpColumns.ERRORS]
        out[IntermediateCSVColumns.SIZE] = line[_OpColumns.SIZE]
        out[IntermediateCSVColumns.WORKERS] = self.tc_map[(actor, op)]

        return out, actor, op


class _OpColumns(CSVColumns):
    _COLUMNS = set()

    TIMESTAMP = None
    ACTOR = None
    THREAD = None
    OPERATION = None
    DURATION = None
    OUTCOME = None
    N = None
    OPS = None
    ERRORS = None
    SIZE = None


class _ClockColumns(CSVColumns):
    _COLUMNS = set()

    CLOCK = None
    NANOSECONDS = None


# OperationThreadCount columns.
class _TCColumns(CSVColumns):
    _COLUMNS = set()

    ACTOR = None
    OPERATION = None
    WORKERS = None


class CSV2:
    """
    Thin object wrapper around a genny metrics' "csv2" file with metadata stored
    as native Python variables.
    """

    def __init__(self, csv2_file_name):
        # parsers for newline-delimited sections in genny's csv2 file.
        header_parsers = {
            'Clocks': self._parse_clocks,
            'OperationThreadCounts': self._parse_thread_count,
            'Operations': self._parse_operations
        }

        # The number to add to the metrics timestamp (a.k.a. c++ system_time) to get
        # the UNIX time.
        self._unix_epoch_offset_ms = None

        # Map of (actor, operation) to thread count.
        self._operation_thread_count_map = {}

        # Store a reader that starts at the first line of actual csv data after the headers
        # have been parsed.
        self._data_reader = None

        # file handle to raw CSV file; lifecycle is shared with this CSV2 object.
        self._csv_file = None

        try:
            # Keep this file open for the duration of this object's lifecycle.
            self._csv_file = open(csv2_file_name, 'r')
            reader = csv.reader(self._csv_file, dialect=_Dialect)

            while True:
                title = next(reader)[0]
                should_stop = header_parsers[title](reader)
                if should_stop:
                    break

        except (IndexError, ValueError) as e:
            raise CSV2ParsingError('Error parsing CSV file: ', csv2_file_name) from e

    def __del__(self):
        if self._csv_file:
            self._csv_file.close()

    def data_reader(self):
        return self._data_reader

    def _parse_clocks(self, reader):
        _ClockColumns.add_columns([header.strip() for header in next(reader)])

        unix_time_line = next(reader)
        metrics_time_line = next(reader)

        if unix_time_line[0] != 'SystemTime' or metrics_time_line[0] != 'MetricsTime':
            raise CSV2ParsingError('Invalid keys for lines: %s %s',
                                   unix_time_line, metrics_time_line)
        unix_time = int(unix_time_line[1])
        metrics_time = int(metrics_time_line[1])

        self._unix_epoch_offset_ms = (unix_time - metrics_time) / (1000 * 1000)

        next(reader)  # Read the blank line.

        return False

    def _parse_thread_count(self, reader):
        _TCColumns.add_columns([h.strip() for h in next(reader)])

        line = next(reader)
        while line:
            actor = line[0]
            op = line[1]
            thread_count = int(line[2])
            self._operation_thread_count_map[(actor, op)] = thread_count
            line = next(reader)

        return False

    def _parse_operations(self, reader):
        _OpColumns.add_columns([h.strip() for h in next(reader)])
        self._data_reader = _OpListReader(reader, self._operation_thread_count_map,
                                          self._unix_epoch_offset_ms)

        return True


class IntermediateCSVColumns(CSVColumns):
    _COLUMNS = set()

    # Declare an explicit default ordering here since this script is writing the intermediate CSV.
    TS_MS = 0
    THREAD = 1
    DURATION = 2
    OUTCOME = 3
    N = 4
    OPS = 5
    ERRORS = 6
    SIZE = 7
    WORKERS = 8

    @classmethod
    def default_columns(cls):
        """
        Ordered list of default columns to write to the CSV, must match the column names in
        the class attributes.
        """
        return ['ts_ms', 'thread', 'duration', 'outcome', 'n', 'ops', 'errors', 'size', 'workers']
