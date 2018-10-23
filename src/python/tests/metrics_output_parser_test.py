"""Tests for metrics_output_parser"""

import os
import unittest

import genny.metrics_output_parser as parser


class GennyOutputParserTest(unittest.TestCase):
    def raises_parse_error(self, input_str):
        with self.assertRaises(parser.ParseError):
            self.parse_string(input_str)

    def parse_string(self, input_str):
        lines = [line.strip() for line in input_str.split("\n")]
        return parser._process_lines(lines, "InputString").timers()

    def parse_file(self, path):
        full_path = os.path.join('.', 'tests', 'fixtures', 'metrics_output_parser-' + path + '.txt')
        return parser.parse(full_path).timers()

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

    def test_fixture1(self):
        actual = self.parse_file('fixture1')
        self.assertEqual(
            actual, {
                'InsertTest.output': {
                    'mean': 1252307.75,
                    'n': 4,
                    'threads': 2,
                    'started': 1537814143293070,
                    'ended': 1537814143687260
                },
                'HelloTest.output': {
                    'mean': 55527.25,
                    'n': 4,
                    'threads': 2,
                    'started': 1537814141128934,
                    'ended': 1537814143457943
                }
            })

    def test_fixture2(self):
        actual = self.parse_file('fixture2')
        self.assertEqual(
            actual, {
                'InsertRemoveTest.remove': {
                    'mean': 4297048.190765498,
                    'n': 823,
                    'threads': 100,
                    'started': 1540233110801455,
                    'ended': 1540233383199723
                },
                'InsertRemoveTest.insert': {
                    'mean': 8656706.697448373,
                    'n': 823,
                    'threads': 100,
                    'started': 1540233103851535,
                    'ended': 1540233380763649
                },
                'Genny.Setup': {
                    'mean': 8694761.0,
                    'n': 1,
                    'threads': 1,
                    'started': 1540233044288445,
                    'ended': 1540233044288445
                }
            })
