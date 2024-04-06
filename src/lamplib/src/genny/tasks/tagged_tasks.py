from typing import List
import os
import sys
import yaml
import structlog
from shrub.v3.evg_project import EvgProject
from shrub.v3.evg_task import EvgTask


from genny.tasks.auto_tasks import (
    CurrentBuildInfo,
    WorkloadLister,
    Repo,
    ConfigWriter,
    YamlReader,
    VariantTask,
)

SLOG = structlog.get_logger(__name__)

def create_project(
    repo: Repo,
    branch_name: str
):
    tagged_tasks = repo.tagged_tasks(branch_name)
    project = {}
    tasks = []
    for tagged_task in tagged_tasks:
        task = {
            "name": tagged_task.name,
            "priority": 5,
            "tags": tagged_task.tags,
            "commands": [
                {
                    "command": "timeout.update",
                    "params": {
                        "exec_timeout_secs": 86400,
                        "timeout_secs": 7200
                    }
                },
                {
                    "func": "f_run_dsi_workload",
                    "vars": {
                        "test_control": tagged_task.name,
                        "auto_workload_path": tagged_task.workload.relative_path,
                    }
                }
            ]
        }   
        tasks.append(task)
    project["tasks"] = tasks
    return project


def parse_activate_generated_tasks(activate_generated_tasks_param):
    activate_tasks = set()
    if not activate_generated_tasks_param:
        return activate_tasks
    variant_task_names = activate_generated_tasks_param.split(",")
    for name in variant_task_names:
        if ":" not in name:
            raise ValueError(
                "Invalid value for 'activate_generated_tasks' param. Value must be of the form 'variant1:task1,variant2:task2'"
            )
        variant_name, task_name = name.strip().split(":")
        variant_task = VariantTask(variant_name, task_name)
        activate_tasks.add(variant_task)

    return activate_tasks


ALL_BRANCHES = ["v5.0", "v6.0", "v7.0", "v7.3", "v8.0", "master"]

def main(workspace_root: str) -> None:
    lister = WorkloadLister(workspace_root=workspace_root)
    reader = YamlReader()

    repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)
    for branch in ALL_BRANCHES:
        project = create_project(repo, branch)
        os.makedirs(os.path.join(workspace_root, "src", "genny", "sys-perf"), exist_ok=True)
        with open(os.path.join(workspace_root, "src", "genny", "sys-perf", f"genny-tasks-{branch}.yml"), "w") as outfile:
            outfile.write(yaml.dump(project))
