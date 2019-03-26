import json
import os
import tempfile
import unittest

from genny.legacy_report import main__legacy_report


class LegacyReportTest(unittest.TestCase):

    @staticmethod
    def get_fixture(*file_path):
        return os.path.join('tests', 'fixtures', 'cedar', *file_path)

    def test_legacy_report(self):
        expected_json = {
            'results': [{
                'name': 'HelloWorld.Greetings',
                'workload': 'HelloWorld.Greetings',
                'start': 419.99968,
                'end': 419.99981,
                'results': {
                    '1': {
                        'ops_per_sec': 13.0,
                        'ops_per_sec_values': [13.0]
                    }
                }
            }, {
                'name': 'InsertRemove.Remove',
                'workload': 'InsertRemove.Remove',
                'start': 419.99983,
                'end': 420.0,
                'results': {
                    '2': {
                        'ops_per_sec': 13.5,
                        'ops_per_sec_values': [13.5]
                    }
                }
            }, {
                'name': 'InsertRemove.Insert',
                'workload': 'InsertRemove.Insert',
                'start': 419.9996,
                'end': 419.99985,
                'results': {
                    '2': {
                        'ops_per_sec': 21.5,
                        'ops_per_sec_values': [21.5]
                    }
                }
            }]
        }

        with tempfile.TemporaryDirectory() as output_dir:
            out_file = os.path.join(output_dir, 'perf.json')
            argv = [self.get_fixture('shared_with_cxx_metrics_test.csv'), '--report-file', out_file]
            main__legacy_report(argv)

            with open(out_file) as f:
                report_json = json.load(f)
                self.assertEqual(report_json, expected_json)
