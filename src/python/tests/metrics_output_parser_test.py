"""Tests for metrics_output_parser"""

import os
import unittest

import genny.metrics_output_parser as parser


class GennyOutputParserTest(unittest.TestCase):
    def parse(self, path):
        full_path = os.path.join('.', 'tests', 'fixtures', 'metrics_output_parser-' + path + '.txt')
        print(os.path.realpath(full_path))
        return parser.parse(full_path)

    def test_fixture1(self):
        actual = self.parse('fixture1')
        self.assertEqual(
            actual.timers(), {
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
        actual = self.parse('fixture2')
        print(actual.timers())
        self.assertEqual(
            actual.timers(), {
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
