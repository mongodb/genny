import json
from subprocess import CalledProcessError
import unittest

from unittest.mock import patch, mock_open
from unittest.mock import Mock
from genny.genny_auto_tasks import construct_all_tasks_json
from genny.genny_auto_tasks import construct_variant_json
from genny.genny_auto_tasks import modified_workload_files
from genny.genny_auto_tasks import validate_user_workloads
from genny.genny_auto_tasks import workload_should_autorun
from genny.genny_auto_tasks import AutoRunSpec
from tests.fixtures.auto_tasks_fixtures import workload_should_autorun_cases


class AutoTasksTest(unittest.TestCase):

    @patch('genny.genny_auto_tasks.open', new_callable=mock_open, read_data='')
    @patch('glob.glob')
    def test_construct_all_tasks_json(self, mock_glob, mock_open):
        """
        This test runs construct_all_tasks_json with static workloads
        and checks that
        the generated json is what evergreen will expect to generate the correct tasks.
        """

        mock_glob.return_value = ["genny/src/workloads/scale/NewWorkload.yml",
                                  "genny/src/workloads/subdir1/subdir2/subdir3/NestedTest.yml",
                                  "/the/full/path/to/genny/src/workloads/execution/ExecutionTask.yml"]

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
                {
                    'name': 'execution_task',
                    'commands': [
                        {
                            'func': 'prepare environment',
                            'vars': {
                                'test': 'execution_task',
                                'auto_workload_path': 'execution/ExecutionTask.yml'
                            }
                        },
                        {'func': 'deploy cluster'},
                        {'func': 'run test'},
                        {'func': 'analyze'}
                    ],
                    'priority': 5
                },
            ],
        }

        actual_json_str = construct_all_tasks_json()
        actual_json = json.loads(actual_json_str)

        self.assertDictEqual(expected_json, actual_json)

    @patch('genny.genny_auto_tasks.modified_workload_files')
    def test_construct_variant_json(self, mock_modified_workload_files):
        """
        This test runs construct_variant_json with static workloads and variants
        and checks that
        the generated json is what evergreen will expect to generate the correct variants.
        """

        mock_modified_workload_files.return_value = ["scale/NewWorkload.yml", "subdir1/subdir2/subdir3/NestedTest.yml",
                                                     "non-yaml-file.md"]
        static_variants = ['variant-1', 'variant-2']

        expected_json = {
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
        actual_json_str = construct_variant_json(workloads, static_variants)
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
        with self.assertRaises(CalledProcessError) as cm:
            modified_workload_files()

    def test_validate_user_workloads(self):
        cases = [
            (
                ['scale/BigUpdate.yml'],
                []
            ),
            (
                ['scale/InsertBigDocs.yml', 'selftests/GennyOverhead.yml'],
                []
            ),
            (
                ['networking/NonExistent.yml'],
                ['no file']
            ),
            (
                ['scale/BigUpdate.yml', 'docs'],
                ['no file']
            ),
            (
                ['CMakeLists.txt'],
                ['not a .yml']
            ),
        ]

        for tc in cases:
            expected = tc[1]
            actual = validate_user_workloads(tc[0])

            # expected is either an empty list (no validation errors), or an ordered list of required substrings of the validation errors.
            self.assertEqual(len(expected), len(actual))
            for idx, expected_err in enumerate(expected):
                self.assertTrue(expected_err in actual[idx])

    def test_workload_should_autorun(self):
        for tc in workload_should_autorun_cases:
            workload_yaml = tc[0]
            env_dict = tc[1]
            expected = tc[2]

            autorun_spec = AutoRunSpec.create_from_workload_yaml(workload_yaml)
            actual = workload_should_autorun(autorun_spec, env_dict)
            self.assertEqual(expected, actual)
