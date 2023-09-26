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
)

SLOG = structlog.get_logger(__name__)

def get_all_builds(global_expansions: dict[str, str], project_file_path: str) -> List[CurrentBuildInfo]:
    """
    Attempts to compute the expansions 
    """
    with open(project_file_path) as project_file:
        project = yaml.safe_load(project_file)
    variants = project["buildvariants"]
    all_builds = []
    for variant in variants:
        if "expansions" in variant and "mongodb_setup" in variant["expansions"]:
            build_info_dict = {}
            build_info_dict.update(global_expansions)
            build_info_dict.update(variant["expansions"])
            build_info_dict["build_variant"] = variant["name"]
            all_builds.append(CurrentBuildInfo(build_info_dict))
    return all_builds

def create_configuration(repo: Repo, builds: List[CurrentBuildInfo], no_activate: bool):
    all_tasks = repo.all_tasks()
    config = Configuration()
    ConfigWriter.configure_all_tasks_modern(config, all_tasks)
    activate_param = False if no_activate else None
    for build in builds:

        build_tasks = repo.variant_tasks(build)
        if len(build_tasks) > 0:
            SLOG.info(f"Generating auto-tasks for variant: {build.variant}")
            ConfigWriter.configure_variant_tasks(config, build_tasks, build.variant, activate=activate_param)
        else:
            SLOG.info(f"No auto-tasks for variant: {build.variant}")
    return config

def main(project_file: str, workspace_root: str, no_activate: bool) -> None:
    reader = YamlReader()
    try:
        global_expansions = reader.load(workspace_root, "expansions.yml")
    except FileNotFoundError:
        SLOG.error(f"Evergreen expansions file {os.path.join(workspace_root, 'expansions.yml')} does not exist. Ensure this file exists and that it is in the correct location.")
        sys.exit(1)
    execution = int(global_expansions["execution"])
    builds = get_all_builds(global_expansions, project_file)

    lister = WorkloadLister(workspace_root=workspace_root)
    repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)

    config = create_configuration(repo, builds, no_activate)
    output_file = os.path.join(workspace_root, "build", "TaskJSON", "Tasks.json")
    ConfigWriter.write_config(execution, config, output_file)
