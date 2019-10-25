import json
import unittest

from unittest.mock import patch
from genny.genny_auto_tasks import construct_task_json


class AutoTasksTest(unittest.TestCase):

    @patch('genny.genny_auto_tasks.modified_workload_files')
    def test_cedar_report(self, mock_modified_workload_files):
        """
        This test runs construct_task_json with static workloads and variants
        and checks that
        the generated json is what evergreen will expect to generate the correct tasks.
        """

        mock_modified_workload_files.return_value = ["scale/NewWorkload.yml", "subdir1/subdir2/subdir3/NestedTest.yml", "non-yaml-file.md"]
        static_variants = ['variant-1', 'variant-2']

        expected_json = {
            'tasks': [
                {
                    'name': 'new_workload',
                    'commands': [
                        {
                            'func': 'prepare environment',
                            'vars': {
                                'test': 'new_workload',
                                'auto_workload_path': 'scale/NewWorkload.yml'
                            }
                        },
                        {'func': 'deploy cluster'},
                        {'func': 'run test'},
                        {'func': 'analyze'}
                    ],
                    'priority': 5
                },
                {
                    'name': 'nested_test',
                    'commands': [
                        {
                            'func': 'prepare environment',
                            'vars': {
                                'test': 'nested_test',
                                'auto_workload_path': 'subdir1/subdir2/subdir3/NestedTest.yml'
                            }
                        },
                        {'func': 'deploy cluster'},
                        {'func': 'run test'},
                        {'func': 'analyze'}
                    ],
                    'priority': 5
                },
            ],
            'buildvariants': [
                {
                    'name': 'variant-1',
                    'tasks': [
                        {'name': 'new_workload'},
                        {'name': 'nested_test'}
                    ]
                },
                {
                    'name': 'variant-2',
                    'tasks': [
                        {'name': 'new_workload'},
                        {'name': 'nested_test'}
                    ]
                }
            ]
        }

        workloads = mock_modified_workload_files()
        actual_json_str = construct_task_json(workloads, static_variants)
        actual_json = json.loads(actual_json_str)

        self.assertDictEqual(expected_json, actual_json)

