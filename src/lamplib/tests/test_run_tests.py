import os
import tempfile
import unittest
from unittest.mock import patch

from tasks.run_tests import cmake_test


class TestRunTests(unittest.TestCase):

    @patch('subprocess.run')
    def test_cmake_test(self, mock_subprocess_run):
        with tempfile.TemporaryDirectory() as temp_dir:
            expected_file = os.path.join(temp_dir, 'build', 'sentinel.junit.xml')
            os.chdir(temp_dir)
            os.mkdir('build')  # Simulate build dir in the genny repo.

            def fail(*args, **kwargs):
                res = unittest.mock.Mock()
                res.returncode = 1
                return res

            mock_subprocess_run.side_effect = fail
            cmake_test(env={})
            self.assertTrue(os.path.isfile(expected_file))

            def succeed(*args, **kwargs):
                res = unittest.mock.Mock()
                res.returncode = 0
                return res

            mock_subprocess_run.side_effect = succeed
            cmake_test(env={})
            self.assertFalse(os.path.isfile(expected_file))
