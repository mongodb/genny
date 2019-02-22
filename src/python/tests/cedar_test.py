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
from collections import OrderedDict as od
from os.path import join as pjoin

from bson import CodecOptions, decode_file_iter

from genny import cedar
from genny.csv2 import CSV2


class CedarTest(unittest.TestCase):

    @staticmethod
    def get_fixture(*csv_file_path):
        return pjoin('tests', 'fixtures', *csv_file_path)

    def verify_output(self, bson_metrics_file_name, expected_results):
        with open(bson_metrics_file_name, 'rb') as f:
            options = CodecOptions(document_class=od)
            index = 0
            for doc in decode_file_iter(f, options):
                self.assertEqual(doc, expected_results[index])
                index += 1

    def test_cedar_main(self):
        expected_result = od([
            ('ts', 10007),
            ('id', 0),
            ('counters', od([('n', 1), ('ops', 6), ('size', 40), ('errors', 2)])),
            ('timers', od([('duration', 100), ('total', 100)])),
            ('gauges', od([('workers', 5)]))
        ])

        with tempfile.TemporaryDirectory() as output_dir:
            args = [
                self.get_fixture('csv2', 'barebones.csv'),
                output_dir
            ]

            # cedar.main__cedar(args)

            # TODO uncomment when everything's done.
            # self.verify_output(pjoin(output_dir, 'MyActor-MyOperation.bson'), expected_result)

    def test_split_csv2(self):
        large_precise_float = 10 ** 15
        mock_data_reader = [
            (['first' for _ in range(10)], 'a1', 'o1'),
            ([1 for _ in range(10)], 'a1', 'o1'),
            # Store a large number to make sure precision is not lost. Python csv converts
            # numbers to floats by default, which has 2^53 or 10^15 precision. Unix time in
            # milliseconds is currently 10^13.
            ([large_precise_float for _ in range(10)], 'a2', 'o2')

        ]
        with tempfile.TemporaryDirectory() as output_dir:
            output_files = cedar.split_into_actor_operation_csv_files(mock_data_reader, output_dir)
            self.assertEqual(output_files, ['a1-o1.csv', 'a2-o2.csv'])

            a1o1 = pjoin(output_dir, output_files[0])
            self.assertTrue(os.path.isfile(a1o1))
            with open(a1o1) as f:
                ll = list(csv.reader(f, quoting=csv.QUOTE_NONNUMERIC))
                self.assertEqual(len(ll), 2)
                self.assertEqual(len(ll[0]), 10)

            a2o2 = pjoin(output_dir, output_files[1])
            self.assertTrue(os.path.isfile(a2o2))
            with open(a2o2) as f:
                ll = list(csv.reader(f, quoting=csv.QUOTE_NONNUMERIC))
                self.assertEqual(len(ll), 1)
                self.assertEqual(ll[0][0], large_precise_float)
                self.assertEqual(len(ll[0]), 10)

    def test_sort_csv(self):
        file_name = 'intermediate_unsorted.csv'
        with tempfile.TemporaryDirectory() as output_dir:
            # Copy the file into a temp dir so we can do in-place sorting.
            shutil.copy(self.get_fixture('cedar', file_name), output_dir)
            cedar.sort_csv_files([file_name], output_dir)
            with open(pjoin(output_dir, file_name)) as f:
                ll = list(csv.reader(f, quoting=csv.QUOTE_NONNUMERIC))
                self.assertEqual(ll[0][0], 10000)
                self.assertEqual(ll[-1][0], 10573)
