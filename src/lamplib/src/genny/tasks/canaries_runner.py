import os
import structlog
from typing import List

from genny.cmd_runner import run_command
from genny.curator import poplar_grpc

SLOG = structlog.get_logger(__name__)


def main_canaries_runner(
    canary_args: List[str], cleanup_metrics: bool, workspace_root: str, genny_repo_root: str
):
    """
    Intended to be the main entry point for running canaries.
    """
    with poplar_grpc(
        cleanup_metrics=cleanup_metrics,
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
    ):
        path = os.path.join(genny_repo_root, "dist", "bin", "genny-canaries")
        if not os.path.exists(path):
            SLOG.error("genny-canaries not found. Run install first.", path=path)
            raise Exception(f"genny-canaries not found at {path}.")
        cmd = [path, *canary_args]

        run_command(
            cmd=cmd, capture=False, check=True, cwd=workspace_root,
        )
