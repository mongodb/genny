import glob
import os
import structlog

from tasks import genny_runner
from toolchain import toolchain_info

SLOG = structlog.get_logger(__name__)


def dry_run_workload(yaml_file_path: str, is_darwin: bool, genny_repo_root: str):
    if is_darwin and os.path.basename(yaml_file_path) in [
        "AuthNInsert.yml",
        "ParallelInsert.yml",
        "LoggingActorExample.yml",
    ]:
        SLOG.info("TIG-1435 skipping dry run on macOS", file=yaml_file_path)
        return

    genny_runner.main_genny_runner(
        genny_args=["dry-run", yaml_file_path],
        genny_repo_root=genny_repo_root,
        cleanup_metrics=True,
    )


def dry_run_workloads(genny_repo_root: str, workspace_root: str):
    info = toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)

    glob_pattern = os.path.join(genny_repo_root, "src", "workloads", "*", "*.yml")
    workloads = glob.glob(glob_pattern)
    curr = 0
    for workload in workloads:
        SLOG.info("Checking workload", workload=workload, index=curr, of_how_many=len(workloads))
        dry_run_workload(
            yaml_file_path=workload, is_darwin=info.is_darwin, genny_repo_root=genny_repo_root
        )
        curr += 1
