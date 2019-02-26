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
import os
import shutil
import tempfile
import unittest
from collections import OrderedDict
from datetime import datetime
from os.path import join as pjoin

from bson import CodecOptions, decode_file_iter

import genny.csv2
from genny import cedar


def _get_fixture(*csv_file_path):
    return pjoin('tests', 'fixtures', *csv_file_path)


class CedarTest(unittest.TestCase):

    def test_split_csv2(self):
        large_precise_float = 10 ** 15
        num_cols = len(genny.csv2.IntermediateCSVColumns.default_columns())
        mock_data_reader = [
            (['first' for _ in range(num_cols)], 'a1'),
            ([1 for _ in range(num_cols)], 'a1'),
            # Store a large number to make sure precision is not lost. Python csv converts
            # numbers to floats by default, which has 2^53 or 10^15 precision. Unix time in
            # milliseconds is currently 10^13.
            ([large_precise_float for _ in range(num_cols)], 'a2')

        ]
        with tempfile.TemporaryDirectory() as output_dir:
            output_files = cedar.split_into_actor_csv_files(mock_data_reader, output_dir)
            self.assertEqual(output_files, ['a1.csv', 'a2.csv'])

            a1o1 = pjoin(output_dir, output_files[0])
            self.assertTrue(os.path.isfile(a1o1))
            with open(a1o1) as f:
                ll = list(csv.reader(f, quoting=csv.QUOTE_NONNUMERIC))
                self.assertEqual(len(ll), 3)
                self.assertEqual(len(ll[0]), 11)

            a2o2 = pjoin(output_dir, output_files[1])
            self.assertTrue(os.path.isfile(a2o2))
            with open(a2o2) as f:
                ll = list(csv.reader(f, quoting=csv.QUOTE_NONNUMERIC))
                self.assertEqual(len(ll), 2)
                self.assertEqual(ll[1][0], large_precise_float)
                self.assertEqual(len(ll[0]), 11)


class CedarIntegrationTest(unittest.TestCase):
    def verify_output(self, bson_metrics_file_name, expected_results, check_last_row_only=False):
        """
        :param bson_metrics_file_name:
        :param expected_results:
        :param check_last_row_only: Check that the last row is correct. Since the results are
        cumulative, this likely means previous rows are all correct as well.
        :return:
        """
        with open(bson_metrics_file_name, 'rb') as f:
            options = CodecOptions(document_class=OrderedDict)
            index = 0
            if check_last_row_only:
                decoded_bson = list(decode_file_iter(f, options))
                self.assertEqual(expected_results, decoded_bson[-1])
            else:
                for doc in decode_file_iter(f, options):
                    self.assertEqual(doc, expected_results[index])
                    index += 1

    def test_cedar_main(self):
        expected_result_insert = OrderedDict([
            ('ts', datetime(1970, 1, 1, 2, 46, 40)),
            ('id', 0),
            ('counters', OrderedDict([
                ('n', 9),
                ('ops', 58),
                ('size', 350),
                ('errors', 23)
            ])),
            ('timers', OrderedDict([
                ('duration', 1320),
                ('total', 1518)
            ])),
            ('gauges', OrderedDict([('workers', 5)]))
        ])

        expected_result_remove = OrderedDict([
            ('ts', datetime(1970, 1, 1, 2, 46, 40)),
            ('id', 0),
            ('counters', OrderedDict([
                ('n', 9),
                ('ops', 58),
                ('size', 257),
                ('errors', 25)
            ])),
            ('timers', OrderedDict([
                ('duration', 1392),
                ('total', 0)
            ])),
            ('gauges', OrderedDict([('workers', 5)]))
        ])

        with tempfile.TemporaryDirectory() as output_dir:
            args = [
                _get_fixture('cedar', 'two_op.csv'),
                output_dir
            ]

            cedar.main__cedar(args)

            self.verify_output(
                pjoin(output_dir, 'InsertRemove-Insert.bson'),
                expected_result_insert,
                check_last_row_only=True
            )

            self.verify_output(
                pjoin(output_dir, 'InsertRemove-Remove.bson'),
                expected_result_remove,
                check_last_row_only=True
            )

    def test_cedar_main_2(self):
        expected_result_greetings = OrderedDict([
            # The operation duration can be ignored because they're a few ns.
            ('ts', datetime.utcfromtimestamp(42 / 1000)),
            ('id', 3),
            ('counters', OrderedDict([
                ('n', 2),
                ('ops', 0),
                ('size', 0),
                ('errors', 0)
            ])),
            ('timers', OrderedDict([
                ('duration', 13),
                ('total', 13)
            ])),
            ('gauges', OrderedDict([('workers', 1)]))
        ])

        expected_result_insert = [
            OrderedDict([
                ('ts', datetime.utcfromtimestamp(42 / 1000)),
                ('id', 1),
                ('counters', OrderedDict([
                    ('n', 1),
                    ('ops', 9),
                    ('size', 300),
                    ('errors', 0)
                ])),
                ('timers', OrderedDict([
                    ('duration', 23),
                    ('total', 23)
                ])),
                ('gauges', OrderedDict([('workers', 2)]))
            ]),
            OrderedDict([
                ('ts', datetime.utcfromtimestamp(42 / 1000)),
                ('id', 2),
                ('counters', OrderedDict([
                    ('n', 2),
                    ('ops', 17),
                    ('size', 500),
                    ('errors', 0)
                ])),
                ('timers', OrderedDict([
                    ('duration', 43),
                    ('total', 43)
                ])),
                ('gauges', OrderedDict([('workers', 2)]))
            ]),
        ]

        with tempfile.TemporaryDirectory() as output_dir:
            args = [
                _get_fixture('cedar', 'shared_with_cxx_metrics_test.csv'),
                output_dir
            ]

            cedar.main__cedar(args)

            self.verify_output(
                pjoin(output_dir, 'HelloWorld-Greetings.bson'),
                [expected_result_greetings],
            )

            self.verify_output(
                pjoin(output_dir, 'InsertRemove-Insert.bson'),
                expected_result_insert,
            )
