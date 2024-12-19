from typing import Optional

import structlog
import os
import sys

from genny.cmd_runner import run_command
from genny.curator import poplar_grpc, calculate_rollups
from genny.tasks import preprocess

SLOG = structlog.get_logger(__name__)


def main_genny_runner(
    workload_yaml_path,
    mongo_uri,
    verbosity,
    override,
    dry_run,
    smoke_test,
    genny_repo_root: str,
    cleanup_metrics: bool,
    workspace_root: str,
    should_calculate_rollups: bool,
    hang: bool = False,
    mongostream_uri: Optional[str] = None,
):
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc(
        cleanup_metrics=cleanup_metrics,
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
    ):
        # Inserting an Info log to gather some information 
        # about what version of Python Genny is running
        # This will help inform whether upgrading packages that deprecate
        # use of Python versions that Genny supports will cause problems
        SLOG.info(f"Python version being used by Genny", python_version=f'"{sys.version}"')

        path = os.path.join(genny_repo_root, "dist", "bin", "genny_core")
        if not os.path.exists(path):
            SLOG.error("genny_core not found. Run install first.", path=path)
            raise Exception(f"genny_core not found at {path}.")
        cmd = [path]

        if dry_run:
            cmd.append("dry-run")
        else:
            cmd.append("run")

        cmd.append("--verbosity")
        cmd.append(verbosity)

        output_dir = os.path.join(workspace_root, "build/WorkloadOutput")
        preprocessed_dir = os.path.join(output_dir, "workload")
        os.makedirs(preprocessed_dir, exist_ok=True)

        processed_workload = os.path.join(preprocessed_dir, os.path.basename(workload_yaml_path))
        with open(processed_workload, "w") as f:
            preprocess.preprocess(
                workload_path=workload_yaml_path,
                default_uri=mongo_uri,
                mongostream_uri=mongostream_uri,
                smoke=smoke_test,
                output_file=f,
                override_file_path=override,
            )

        cmd.append("--workload-file")
        cmd.append(processed_workload)

        if hang:
            import time
            import shlex

            SLOG.info(
                "Debug mode. Poplar is running. "
                "Start genny_core (./build/src/driver/genny_core or ./dist/bin/genny_core) "
                "on your own with the fully processed workload file."
                f"\n\n    {shlex.join(cmd)}\n\n"
                "Ctrl+C here when done."
            )
            while True:
                time.sleep(10)

        run_command(
            cmd=cmd,
            capture=False,
            check=True,
            cwd=workspace_root,
        )
    if should_calculate_rollups:
        calculate_rollups(
            output_dir=output_dir, workspace_root=workspace_root, genny_repo_root=genny_repo_root
        )
