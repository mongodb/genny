import glob
import os

from tasks import genny_runner


def dry_run_workload(yaml_file_path: str, os_family: str, genny_repo_root):
    if os_family == "Darwin" and os.path.basename(yaml_file_path) in [
        "AuthNInsert.yml",
        "ParallelInsert.yml",
    ]:
        print("TIG-1435 skipping dry run on macOS")
        return

    genny_runner.main_genny_runner(
        ["dry-run", yaml_file_path], genny_repo_root=genny_repo_root, cleanup=True
    )


def dry_run_workloads(genny_repo_root: str, os_family):
    glob_pattern = os.path.join(genny_repo_root, "src", "workloads", "*", "*.yml")
    workloads = glob.glob(glob_pattern)
    curr = 1
    for workload in workloads:
        print(f"Checking workload {curr} of {len(workloads)}")
        dry_run_workload(workload, os_family, genny_repo_root)
        curr += 1
