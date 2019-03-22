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

import unittest

import genny.perf_json as perf_json
import tests.parser_test_lib as test_lib


class PerfJsonTests(unittest.TestCase):
    def test_hole_in_thread_numbers(self):
        parsed = test_lib.parse_string("""
        Clocks
        SystemTime,1
        MetricsTime,2

        Timers
        12,A.0.o,50
        13,A.3.o,50
        """)
        actual = perf_json.translate(parsed.timers())
        self.assertEqual(
            actual, {
                'results': [{
                    'end': 0.00012,
                    'name': 'A.o',
                    'results': {
                        2: {
                            'ops_per_sec': 50.0,
                            'ops_per_sec_values': [50.0]
                        }
                    },
                    'start': -0.00039,
                    'workload': 'A.o'
                }],
            })

    def test_fixture2(self):
        parsed = test_lib.parse_fixture('csvoutput2')
        actual = perf_json.translate(parsed.timers())
        self.assertEqual(
            actual, {
                'results': [{
                    'name': 'InsertRemoveTest.remove',
                    'workload': 'InsertRemoveTest.remove',
                    'start': 15402331038.70294,
                    'end': 15402333831.99723,
                    'results': {
                        100: {
                            'ops_per_sec': 4297048.190765492,
                            'ops_per_sec_values': [4297048.190765492]
                        }
                    }
                },
                    {
                        'name': 'InsertRemoveTest.insert',
                        'workload': 'InsertRemoveTest.insert',
                        'start': 15402330730.74953,
                        'end': 15402333807.63649,
                        'results': {
                            100: {
                                'ops_per_sec': 8656706.69744836,
                                'ops_per_sec_values': [8656706.69744836]
                            }
                        }
                    },
                    {
                        'name': 'Genny.Setup',
                        'workload': 'Genny.Setup',
                        'start': 15402330355.93684,
                        'end': 15402330442.88445,
                        'results': {
                            1: {
                                'ops_per_sec': 8694761.0,
                                'ops_per_sec_values': [8694761.0]
                            }
                        }
                    }]
            })

    def test_fixture1(self):
        parsed = test_lib.parse_fixture('csvoutput1')
        actual = perf_json.translate(parsed.timers())
        self.assertEqual(
            actual, {
                'results': [{
                    'name': 'InsertTest.output',
                    'workload': 'InsertTest.output',
                    'start': 15378141410.61109,
                    'end': 15378141436.8726,
                    'results': {
                        2: {
                            'ops_per_sec': 1252307.75,
                            'ops_per_sec_values': [1252307.75]
                        }
                    }
                },
                    {
                        'name': 'HelloTest.output',
                        'workload': 'HelloTest.output',
                        'start': 15378141410.61476,
                        'end': 15378141434.57943,
                        'results': {
                            2: {
                                'ops_per_sec': 55527.25,
                                'ops_per_sec_values': [55527.25]
                            }
                        }
                    }]
            })
