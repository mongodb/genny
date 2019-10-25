import json
from subprocess import CalledProcessError
import unittest

from unittest.mock import patch
from unittest.mock import Mock
from genny.genny_auto_tasks import construct_task_json
from genny.genny_auto_tasks import modified_workload_files


class AutoTasksTest(unittest.TestCase):

    @patch('genny.genny_auto_tasks.modified_workload_files')
    def test_construct_task_json(self, mock_modified_workload_files):
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

    @patch('subprocess.check_output')
    def test_modified_workload_files(self, mock_check_output):
        """
        The test calls modified_workload_files() with a stubbed version of subprocess.check_output returning static git results
        and checks that
        the outputted workload_files are as expected based on the git results.
        """

        # Cases that should be executed successfully.
        cases = [
            (
                b'../workloads/sub/abc.yml\n',
                ['sub/abc.yml']
            ),
            (
                b'../workloads/sub1/foo.yml\n\
                ../workloads/sub2/bar.yml\n\
                ../workloads/a/very/very/nested/file.yml\n',
                ['sub1/foo.yml', 'sub2/bar.yml', 'a/very/very/nested/file.yml']
            ),
            (
                b'',
                []
            ),
        ]

        for tc in cases:
            mock_check_output.return_value = tc[0]
            expected_files = tc[1]
            actual_files = modified_workload_files()
            self.assertEqual(expected_files, actual_files)

        # Check that we handle errors from subprocess.check_output properly.
        mock_check_output.side_effect = CalledProcessError(127, 'cmd')
        with self.assertRaises(SystemExit) as cm:
            modified_workload_files()

