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

"""Tests for metrics_output_parser"""

import unittest

import genny.metrics_output_parser as parser
import tests.parser_test_lib as test_lib


class GennyOutputParserTest(unittest.TestCase):
    def raises_parse_error(self, input_str):
        with self.assertRaises(parser.ParseError):
            test_lib.parse_string(input_str)

    def test_no_clocks(self):
        self.raises_parse_error("""
        Timers
        1234,A.0.o,345
        """)

    def test_missing_clocks(self):
        self.raises_parse_error("""
        Clocks

        Timers
        1234,A.0.o,345
        """)

    def test_timers_before_clocks(self):
        self.raises_parse_error("""
        Timers
        1234,A.0.o,345

        Clocks
        SystemTime,23439048
        MetricsTime,303947
        """)

    def test_csv_no_sections_have_data(self):
        self.assertEqual(
            test_lib.parse_string("""
        Clocks

        Gauges

        Counters

        Timers
        """).timers(), {})

    def test_empty_input(self):
        self.assertEqual(test_lib.parse_string("").timers(), {})

    def test_fixture1(self):
        actual = test_lib.parse_fixture('csvoutput1').timers()
        self.assertEqual(
            actual, {
                'InsertTest.output': {
                    'mean': 1252307.75,
                    'n': 4,
                    'threads': {'id-0', 'id-1'},
                    'started': 1537814141061109,
                    'ended': 1537814143687260
                },
                'HelloTest.output': {
                    'mean': 55527.25,
                    'n': 4,
                    'threads': {'0', '1'},
                    'started': 1537814141061476,
                    'ended': 1537814143457943
                }
            })

    def test_fixture2(self):
        actual = test_lib.parse_fixture('csvoutput2').timers()
        self.assertEqual(
            actual, {
                'InsertRemoveTest.remove': {
                    'n':
                        823,
                    'threads':
                        set([
                            str(x) for x in [
                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                                19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                                36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
                                53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
                                70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
                                87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99
                            ]
                        ]),
                    'started':
                        1540233103870294,
                    'ended':
                        1540233383199723,
                    'mean':
                        4297048.190765492
                },
                'InsertRemoveTest.insert': {
                    'n':
                        823,
                    'threads':
                        set([
                            str(x) for x in [
                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                                19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                                36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
                                53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
                                70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
                                87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99
                            ]
                        ]),
                    'started':
                        1540233073074953,
                    'ended':
                        1540233380763649,
                    'mean':
                        8656706.69744836
                },
                'Genny.Setup': {
                    'n': 1,
                    'threads': {'0'},
                    'started': 1540233035593684,
                    'ended': 1540233044288445,
                    'mean': 8694761.0
                }
            })

    def test_fixture3(self):
        actual = test_lib.parse_fixture('csvoutput3').timers()
        self.assertEqual(
            actual, {
                'InsertRemoveTest.remove.op-time': {
                    'n': 6,
                    'threads': {'id-99'},
                    'started': 1547004939911848888,
                    'ended': 1547004939932315379,
                    'mean': 1814961.8333333333
                }
            })
