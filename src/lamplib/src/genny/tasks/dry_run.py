import glob
import os
import structlog

from genny.tasks import genny_runner
from genny.toolchain import toolchain_info

SLOG = structlog.get_logger(__name__)

""" A list of files to ignore. They are in WIP, not relevant etc..."""
WIP_YML_FILES = ["CrudActorFSMAdvanced.yml"]


def dry_run_workload(
    yaml_file_path: str, is_darwin: bool, genny_repo_root: str, workspace_root: str
):
    yaml_file_basename = os.path.basename(yaml_file_path)

    if yaml_file_basename in WIP_YML_FILES:
        SLOG.info("Skipping dry run for workloads for future functionality.", file=yaml_file_path)
        return

    if yaml_file_basename in [
        "MixedWorkloadsGennyStress.yml",
        "ClusteredCollection.yml",
        "ClusteredCollectionLargeRecordIds.yml",
    ]:
        SLOG.info(f"TIG-3290 skipping dry run for {yaml_file_basename}.", file=yaml_file_path)
        return

    if is_darwin and yaml_file_basename in [
        "AuthNInsert.yml",
        "ParallelInsert.yml",
        "LoggingActorExample.yml",
    ]:
        SLOG.info("TIG-1435 skipping dry run on macOS", file=yaml_file_path)
        return

    genny_runner.main_genny_runner(
        workload_yaml_path=yaml_file_path,
        mongo_uri="mongodb://localhost:27017",
        verbosity="info",
        override=None,
        dry_run=True,
        smoke_test=False,
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        cleanup_metrics=True,
    )


def dry_run_workloads(genny_repo_root: str, workspace_root: str, given_workload: str = None):
    info = toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)

    if given_workload is not None:
        workloads = [given_workload]
    else:
        glob_pattern = os.path.join(genny_repo_root, "src", "workloads", "*", "*.yml")
        workloads = glob.glob(glob_pattern)
    curr = 0
    for workload in workloads:
        SLOG.info("Checking workload", workload=workload, index=curr, of_how_many=len(workloads))
        dry_run_workload(
            yaml_file_path=workload,
            is_darwin=info.is_darwin,
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
        )
        curr += 1
