from typing import List

import structlog
import tempfile
import shutil
import os

from genny.cmd_runner import run_command
from genny.curator import poplar_grpc
from genny.tasks import preprocess

SLOG = structlog.get_logger(__name__)


def main_genny_runner(
    genny_args: List[str],
    genny_repo_root: str,
    cleanup_metrics: bool,
    workspace_root: str,
    hang: bool = False,
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

        preprocessed_dir = os.path.join(workspace_root, "build/WorkloadOutput/workload")
        os.makedirs(preprocessed_dir, exist_ok=True)

        # Intercept the workload given to the core binary.
        index = -1
        if "-w" in cmd:
            index = cmd.index("-w") + 1
        elif "--workload-file" in cmd:
            index = cmd.index("--workload-file") + 1
        elif "dry-run" in cmd:
            index = cmd.index("dry-run") + 1

        if index >= 0:
            workload_path = cmd[index]
            smoke = "-s" in cmd or "--smoke-test" in cmd

            temp_workload = os.path.join(preprocessed_dir, os.path.basename(workload_path))
            with open(temp_workload, "w") as f:
                preprocess.preprocess(workload_path=workload_path, smoke=smoke, output_file=f)
            cmd[index] = temp_workload

        if hang:
            import time

            SLOG.info(
                "Debug mode. Poplar is running. Start cedar_core on your own. Ctrl+C here when done."
            )
            while True:
                time.sleep(10)

        run_command(
            cmd=cmd, capture=False, check=True, cwd=workspace_root,
        )
