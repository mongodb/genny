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

from collections import OrderedDict as od
import os
import tempfile
import unittest

from bson import CodecOptions, decode_file_iter

from genny import cedar


class CedarTest(unittest.TestCase):

    @staticmethod
    def get_fixture(csv_file):
        return os.path.join('fixtures', 'csv2', csv_file + '.csv')

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
                self.get_fixture('barebones'),
                output_dir
            ]

            cedar.main__cedar(args)

            self.verify_output(os.path.join(output_dir, 'MyActor-MyOperation.csv'), expected_result)
