import os
import shutil

import structlog
from git import Repo as GitRepo
from shrub.command import CommandDefinition

from genny.tasks.auto_tasks import (
    ConfigWriter,
    Repo,
    WorkloadLister,
    YamlReader,
)
from genny.tasks.auto_tasks_all import create_configuration, get_all_builds

SLOG = structlog.get_logger("genny.tasks.auto_tasks.local")
DSI_TMP_PATH = "./dsi"
DSI_REPO_URL = "https://github.com/10gen/dsi.git"
PRIVATE_WORKLOADS_TMP_PATH = "./PrivateWorkloads"
PRIVATE_WORKLOADS_REPO_URL = "https://github.com/10gen/PrivateWorkloads.git"
WORKLOAD_FILE_PATTERN = os.path.join("**", "src", "workloads", "**", "*.yml")
FIRST_EXECUTION = 0
WORKLOAD_PATH_KEY = "auto_workload_path"
DSI_TASK_NAME = "f_run_dsi_workload"

PROJECT_FILES = {
    "master": [
        os.path.join("evergreen", "system_perf", "master", "variants.yml"),
        os.path.join("evergreen", "system_perf", "master", "master_variants.yml"),
    ],
    "v8.0": [
        os.path.join("evergreen", "system_perf", "8.0", "variants.yml"),
    ],
    "v7.0": [
        os.path.join("evergreen", "system_perf", "7.0", "variants.yml"),
    ],
    "v6.0": [
        os.path.join("evergreen", "system_perf", "6.0", "variants.yml"),
    ],
    "v5.0": [
        os.path.join("evergreen", "system_perf", "5.0", "variants.yml"),
    ],
}


def get_builds(branch_name: str, dsi_path: str) :
    builds = []
    expansions = {"branch_name": branch_name, "execution": FIRST_EXECUTION}
    for project_file in PROJECT_FILES[branch_name]:
        project_file_path = os.path.join(dsi_path, project_file)
        builds.extend(get_all_builds(expansions, project_file_path, False))

    return builds


def fix_auto_workload_path(command: CommandDefinition):
    if command._vars[WORKLOAD_PATH_KEY].startswith("src"):
        command._vars[WORKLOAD_PATH_KEY] = "src/genny/" + command._vars[WORKLOAD_PATH_KEY]
    if command._vars[WORKLOAD_PATH_KEY].startswith("PrivateWorkloads"):
        command._vars[WORKLOAD_PATH_KEY] = "src/" + command._vars[WORKLOAD_PATH_KEY]


def set_up_environment(dsi_path, private_workloads_path):
    if not dsi_path:
        SLOG.info("Cloning the DSI repo to look for variants")
        GitRepo.clone_from(DSI_REPO_URL, DSI_TMP_PATH)
        dsi_path = DSI_TMP_PATH
    if not private_workloads_path:
        SLOG.info("Cloning the PrivateWorkloads repo to look for tests")
        GitRepo.clone_from(PRIVATE_WORKLOADS_REPO_URL, PRIVATE_WORKLOADS_TMP_PATH)
        private_workloads_path = PRIVATE_WORKLOADS_TMP_PATH
    else:
        SLOG.info("Creating temporary symlink to the PrivateWorkloads repository")
        os.symlink(private_workloads_path, PRIVATE_WORKLOADS_TMP_PATH)
    return dsi_path


def cleanup_environment():
    if os.path.exists(DSI_TMP_PATH):
        SLOG.info("Removing the temporary DSI repo")
        shutil.rmtree(DSI_TMP_PATH)
    if os.path.exists(PRIVATE_WORKLOADS_TMP_PATH):
        if os.path.islink(PRIVATE_WORKLOADS_TMP_PATH):
            SLOG.info("Removing the temporary symbolic link for the PrivateWorkloads repo")
            os.remove(PRIVATE_WORKLOADS_TMP_PATH)
        else:
            SLOG.info("Removing the temporary PrivateWorkloads repo")
            shutil.rmtree(PRIVATE_WORKLOADS_TMP_PATH)


def main(workspace_root: str, dsi_path: str = None, private_workloads_path: str = None) -> None:
    dsi_path = set_up_environment(dsi_path, private_workloads_path)

    for branch_name in PROJECT_FILES:
        SLOG.info("Creating the configuration", branch_name=branch_name)
        builds = get_builds(branch_name, dsi_path)
        lister = WorkloadLister(
            workspace_root=workspace_root, workload_file_pattern=WORKLOAD_FILE_PATTERN
        )
        repo = Repo(lister=lister, reader=YamlReader(), workspace_root=workspace_root)
        config = create_configuration(repo, builds, False, [])

        SLOG.info("Removing global exec_timeout_secs as it is already in the main project")
        config._exec_timeout_secs = None

        SLOG.info("Fixing the auto_workload_path to match the DSI folder structure")
        for task in config._tasks:
            for command in task._commands._cmd_seq:
                if command._function_name == DSI_TASK_NAME:
                    fix_auto_workload_path(command)

        SLOG.info("Writing the configuration", branch_name=branch_name)
        output_file = os.path.join("evergreen", "system_perf", branch_name.replace("v", ""), "genny_tasks.yml")
        ConfigWriter.write_config(
            execution=FIRST_EXECUTION,
            config=config,
            output_file=output_file,
            file_format=ConfigWriter.FileFormat.YAML,
        )

    cleanup_environment()
