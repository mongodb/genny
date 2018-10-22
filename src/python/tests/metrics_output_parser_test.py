"""Tests for metrics_output_parser"""

import os
import unittest

import genny.metrics_output_parser as parser


class GennyOutputParserTest(unittest.TestCase):
    def parse(self, path):
        full_path = os.path.join('.', 'tests', 'fixtures', 'metrics_output_parser-' + path + '.txt')
        print(os.path.realpath(full_path))
        return parser.parse(full_path)

    def testBasicInput(self):
        actual = self.parse('fixture1')
        print(actual.timers())
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
