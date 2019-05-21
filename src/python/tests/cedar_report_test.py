import datetime
import json
import tempfile
import unittest

from unittest.mock import patch

from genny.cedar_report import CertRetriever, main__cedar_report
from tests.cedar_test import get_fixture


class _NoopCertRetriever(CertRetriever):
    @staticmethod
    def _fetch(url, output, **kwargs):
        return output


_FIXED_DATETIME = datetime.datetime(year=2000, month=1, day=1).isoformat()


class CedarReportTest(unittest.TestCase):

    @patch('genny.cedar_report.ShellCuratorRunner.run')
    def test_cedar_report(self, mock_uploader_run):
        """
        This test documents the environment variables needed to run cedar_report.py and checks that
        the environment variables are correctly used.
        """
        class MatchAnyString(object):
            def __eq__(self, other):
                return type(other) == str

        mock_env = {
            'task_name': 'my_task_name',
            'project': 'my_project',
            'version_id': 'my_version',
            'revision_order_id': '1',
            'build_variant': 'my_variant',
            'task_id': 'my_task_id',
            'execution': '1',
            'is_patch': 'true',  # This should get converted to mainline = False in the report.

            'test_name': 'my_test_name',

            'perf_jira_user': 'my_username',
            'perf_jira_pw': 'my_password',

            'terraform_key': 'my_aws_key',
            'terraform_secret': 'my_aws_secret'
        }

        expected_uploader_run_args = [
            'curator', 'poplar', 'send', '--service', 'cedar.mongodb.com:7070', '--cert',
            'cedar.user.crt', '--key', 'cedar.user.key', '--ca', 'cedar.ca.pem', '--path',
            'cedar_report.json']

        expected_json = {
            'project': 'my_project',
            'version': 'my_version',
            'revision_order_id': 1,
            'variant': 'my_variant',
            'task_name': 'my_task_name',
            'task_id': 'my_task_id',
            'execution_number': 1,
            'mainline': False,
            'tests': [{
                'info': {
                    'test_name': 'my_task_name',
                    'trial': 0,
                    'tags': [],
                    'args': {}
                },
                'created_at': _FIXED_DATETIME,
                'completed_at': _FIXED_DATETIME,
                'artifacts': [],
                'metrics': None,
                'sub_tests': [{
                    'info': {
                        'test_name': 'HelloWorld-Greetings',
                        'trial': 0,
                        'tags': [],
                        'args': {}
                    },
                    'created_at': MatchAnyString(),
                    'completed_at': MatchAnyString(),
                    'artifacts': [{
                        'bucket': 'genny-metrics',
                        'path': 'HelloWorld-Greetings',
                        'tags': [],
                        'local_path': MatchAnyString(),
                        'created_at': MatchAnyString(),
                        'convert_bson_to_ftdc': True,
                        'permissions': 'public-read',
                        'prefix': 'my_task_id_1'
                    }],
                    'metrics': None,
                    'sub_tests': None
                }, {
                    'info': {
                        'test_name': 'InsertRemove-Insert',
                        'trial': 0,
                        'tags': [],
                        'args': {}
                    },
                    'created_at': MatchAnyString(),
                    'completed_at': MatchAnyString(),
                    'artifacts': [{
                        'bucket': 'genny-metrics',
                        'path': 'InsertRemove-Insert',
                        'tags': [],
                        'local_path': MatchAnyString(),
                        'created_at': MatchAnyString(),
                        'convert_bson_to_ftdc': True,
                        'permissions': 'public-read',
                        'prefix': 'my_task_id_1'
                    }],
                    'metrics': None,
                    'sub_tests': None
                }, {
                    'info': {
                        'test_name': 'InsertRemove-Remove',
                        'trial': 0,
                        'tags': [],
                        'args': {}
                    },
                    'created_at': MatchAnyString(),
                    'completed_at': MatchAnyString(),
                    'artifacts': [{
                        'bucket': 'genny-metrics',
                        'path': 'InsertRemove-Remove',
                        'tags': [],
                        'local_path': MatchAnyString(),
                        'created_at': MatchAnyString(),
                        'convert_bson_to_ftdc': True,
                        'permissions': 'public-read',
                        'prefix': 'my_task_id_1'
                    }],
                    'metrics': None,
                    'sub_tests': None
                }]
            }],
            'bucket': {
                'api_key': 'my_aws_key',
                'api_secret': 'my_aws_secret',
                'api_token': None,
                'region': 'us-east-1',
                'name': 'genny-metrics',
            }
        }

        with tempfile.TemporaryDirectory() as output_dir:
            argv = [get_fixture('cedar', 'shared_with_cxx_metrics_test.csv'), output_dir]
            main__cedar_report(argv, mock_env, _NoopCertRetriever)

        with open('cedar_report.json') as f:
            report_json = json.load(f)

            # Verify just the top-level date fields for correct formatting.
            report_json['tests'][0]['created_at'] = _FIXED_DATETIME
            report_json['tests'][0]['completed_at'] = _FIXED_DATETIME

            self.assertDictEqual(expected_json, report_json)

        mock_uploader_run.assert_called_with(expected_uploader_run_args)
