"""Tests for genny/src/lamplib/src/genny/cli.py"""
import tempfile
import unittest
import os
from genny import cli
from pathlib import Path


class CtxMock(dict):
    """Mock out the click context"""

    pass


class CLITestCase(unittest.TestCase):
    """Unit tests for CLI operations"""

    def test_init_workload_repo(self):
        ctx = CtxMock()
        with tempfile.TemporaryDirectory() as tmp_genny_dir:
            genny_dsi_dir = os.path.join(tmp_genny_dir, "dsi")
            os.makedirs(genny_dsi_dir)
            Path(os.path.join(genny_dsi_dir, "run-from-dsi")).touch()
            ctx.obj = {"DEBUG": False, "GENNY_REPO_ROOT": tmp_genny_dir}
            with tempfile.TemporaryDirectory() as tmp_workload_dir:
                cli._init_workload_repo(ctx, tmp_workload_dir)
                assert os.path.exists(os.path.join(tmp_workload_dir, "dsi", "run-from-dsi"))
                assert os.path.exists(os.path.join(tmp_workload_dir, "src", "workloads", "docs"))
