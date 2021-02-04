from typing import List

import structlog
import os

from genny.cmd_runner import run_command
from genny.curator import poplar_grpc

SLOG = structlog.get_logger(__name__)


def main_genny_runner(
    genny_args: List[str], genny_repo_root: str, cleanup_metrics: bool, workspace_root: str
):
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc(
        cleanup_metrics=cleanup_metrics,
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
    ):
        path = os.path.join(genny_repo_root, "dist", "bin", "genny_core")
        if not os.path.exists(path):
            SLOG.error("genny_core not found. Run install first.", path=path)
            raise Exception(f"genny_core not found at {path}.")
        cmd = [path, *genny_args]

        run_command(
            cmd=cmd, capture=False, check=True, cwd=workspace_root,
        )
