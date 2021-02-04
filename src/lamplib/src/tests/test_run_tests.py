import os
import tempfile
import shutil
import unittest
from unittest.mock import patch

from genny.tasks import run_tests
from genny import curator, cmd_runner, toolchain


class TestRunTests(unittest.TestCase):
    def setUp(self):
        self.workspace_root = tempfile.mkdtemp()

    def cleanUp(self):
        shutil.rmtree(self.workspace_root)

    @patch.object(curator, "poplar_grpc", spec=curator.poplar_grpc, name="poplar")
    @patch.object(toolchain, "toolchain_info", spec=toolchain.toolchain_info, name="toolchain")
    @patch.object(cmd_runner, "run_command", spec=cmd_runner.run_command, name="cmd_runner")
    def test_cmake_test(self, mock_run_command, mock_toolchain, mock_poplar_grpc):
        expected_file = os.path.join(self.workspace_root, "build", "XUnitXML", "sentinel.junit.xml")

        def fail(cmd, cwd, env, capture, check):
            return cmd_runner.RunCommandOutput(returncode=1, stdout=[], stderr=[])

        def succeed(cmd, cwd, env, capture, check):
            return cmd_runner.RunCommandOutput(returncode=0, stdout=[], stderr=[])

        mock_run_command.side_effect = fail
        run_tests.cmake_test(genny_repo_root=".", workspace_root=self.workspace_root)
        assert os.path.isfile(expected_file), f"{expected_file} must exist"

        mock_run_command.side_effect = succeed
        run_tests.cmake_test(genny_repo_root=".", workspace_root=self.workspace_root)
        self.assertFalse(os.path.isfile(expected_file))
