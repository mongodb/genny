from typing import List
import os
import sys
import yaml
import structlog
from shrub.config import Configuration

from genny.tasks.auto_tasks import (
    CurrentBuildInfo,
    WorkloadLister,
    Repo,
    ConfigWriter,
    YamlReader,
    VariantTask,
)

SLOG = structlog.get_logger(__name__)


def get_all_builds(
    global_expansions: dict[str, str], project_file_path: str
) -> List[CurrentBuildInfo]:
    """
    Attempts to compute the expansions
    """
    with open(project_file_path) as project_file:
        project = yaml.safe_load(project_file)
    variants = project["buildvariants"]
    all_builds = []
    for variant in variants:
        contains_auto_task = any(task["name"] == "schedule_variant_auto_tasks" for task in variant["tasks"])
        if "expansions" in variant and "mongodb_setup" in variant["expansions"] and contains_auto_task:
            build_info_dict = {}
            build_info_dict.update(global_expansions)
            build_info_dict.update(variant["expansions"])
            build_info_dict["build_variant"] = variant["name"]
            all_builds.append(CurrentBuildInfo(build_info_dict))
    return all_builds


def create_configuration(
    repo: Repo, builds: List[CurrentBuildInfo], no_activate: bool, activate_tasks: List[VariantTask]
):
    all_tasks = repo.all_tasks()
    config = Configuration()
    ConfigWriter.configure_all_tasks_modern(config, all_tasks)
    activate_all_param = False if no_activate else None
    for build in builds:

        build_tasks = repo.variant_tasks(build)
        if len(build_tasks) > 0:
            SLOG.info(f"Generating auto-tasks for variant: {build.variant}")
            ConfigWriter.configure_variant_tasks(
                config,
                build_tasks,
                build.variant,
                activate_all=activate_all_param,
                activate_tasks=activate_tasks,
            )
        else:
            SLOG.info(f"No auto-tasks for variant: {build.variant}")
    return config


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


def main(project_files: List[str], workspace_root: str, no_activate: bool) -> None:
    reader = YamlReader()
    try:
        global_expansions = reader.load(workspace_root, "expansions.yml")
    except FileNotFoundError:
        SLOG.error(
            f"Evergreen expansions file {os.path.join(workspace_root, 'expansions.yml')} does not exist. Ensure this file exists and that it is in the correct location."
        )
        sys.exit(1)
    execution = int(global_expansions["execution"])

    activate_tasks = parse_activate_generated_tasks(
        global_expansions.get("activate_generated_tasks", "")
    )

    builds = []
    for project_file in project_files:
        builds.extend(get_all_builds(global_expansions, project_file))

    workload_file_pattern = os.path.join(workspace_root, "src", "*", "src", "workloads", "**", "*.yml")
    lister = WorkloadLister(workspace_root=workspace_root, workload_file_pattern=workload_file_pattern)
    repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)

    config = create_configuration(repo, builds, no_activate, activate_tasks)
    output_file = os.path.join(workspace_root, "build", "TaskJSON", "Tasks.json")
    ConfigWriter.write_config(execution, config, output_file, ConfigWriter.FileFormat.JSON)
