"""
Generates evergreen tasks based on the current state of the repo.
"""

import enum
import glob
import os
import re
import sys
from typing import NamedTuple, List, Optional, Set
import yaml
import structlog

from shrub.command import CommandDefinition
from shrub.config import Configuration
from shrub.variant import TaskSpec

from genny.cmd_runner import run_command

SLOG = structlog.get_logger(__name__)


#
# The classes are listed here in dependency order to avoid having to quote typenames.
#
# For comprehension, start at main(), then class Workload, then class Repo. Rest
# are basically just helpers.
#


class YamlReader:
    # You could argue that YamlReader, WorkloadLister, and maybe even Repo
    # should be the same class - perhaps renamed to System or something?
    # Maybe make these methods static to avoid having to pass an instance around.
    def load(self, workspace_root: str, path: str) -> dict:
        """
        :param workspace_root: effective cwd
        :param path: path relative to workspace_root
        :return: deserialized yaml file
        """
        joined = os.path.join(workspace_root, path)
        with open(joined) as handle:
            return yaml.safe_load(handle)

    # Really just here for easy mocking.
    def exists(self, path: str) -> bool:
        return os.path.exists(path)


class WorkloadLister:
    """
    Lists files in the repo dir etc.
    Separate from the Repo class for easier testing.
    """

    def __init__(self, workspace_root: str, workload_file_pattern: str):
        self.workspace_root = workspace_root
        self.workload_file_pattern = workload_file_pattern

    def all_workload_files(self) -> List[str]:
        return sorted(list(set(glob.glob(self.workload_file_pattern, recursive=True))))

    def modified_workload_files(self) -> Set[str]:
        """Relies on git to find files in src/workloads modified versus origin/master"""
        src_path = os.path.join(self.workspace_root, "src")
        all_repo_directories = {
            path for path in os.listdir(src_path) if os.path.isdir(os.path.join(src_path, path))
        }
        command = (
            "git diff --name-only --diff-filter=AMR "
            "$(git merge-base HEAD origin) -- src/workloads/"
        )
        modified_workloads = set()
        for repo_directory in all_repo_directories:
            repo_path = os.path.join(src_path, repo_directory)
            cmd = run_command(cmd=[command], cwd=repo_path, shell=True, check=True)
            if cmd.returncode != 0:
                SLOG.fatal("Failed to compare workload directory to origin.", *cmd)
                raise RuntimeError(
                    "Failed to compare workload directory to origin: stdout: {cmd.stdout} stderr: {cmd.stderr}"
                )
            lines = cmd.stdout
            modified_workloads.update(
                {os.path.join(repo_path, line) for line in lines if line.endswith(".yml")}
            )
        return modified_workloads


class OpName(enum.Enum):
    """
    What kind of tasks we're generating in this invocation.
    """

    ALL_TASKS = object()
    VARIANT_TASKS = object()
    PATCH_TASKS = object()

    @staticmethod
    def from_flag(mode_name):
        if mode_name == "all_tasks":
            return OpName.ALL_TASKS
        if mode_name == "patch_tasks":
            return OpName.PATCH_TASKS
        if mode_name == "variant_tasks":
            return OpName.VARIANT_TASKS


class CurrentBuildInfo:
    def __init__(self, expansions: dict[str, str]):
        self.conts = expansions
        self.variant = expansions["build_variant"]
        self.execution = int(expansions["execution"])

    def has(self, key: str, acceptable_values: List[str]) -> bool:
        """
        :param key: a key from environment (expansions.yml, bootstrap.yml, etc)
        :param acceptable_values: possible values we accept
        :return: if the actual value from env[key] is in the list of acceptable values
        """
        if key not in self.conts:
            return False
        actual = self.conts[key]
        return any(actual == acceptable_value for acceptable_value in acceptable_values)

    def __eq__(self, other):
        return self.conts == other.conts

    def __repr__(self):
        return f"CurrentBuildInfo: {self.conts}"


class VariantTask(NamedTuple):
    variant: str
    task: str


class GeneratedTask(NamedTuple):
    name: str
    bootstrap_key: Optional[str]
    bootstrap_value: Optional[str]
    workload: "Workload"


class AutoRunBlock(NamedTuple):
    when: dict
    then_run: dict


class Workload:
    """
    Represents a workload yaml file.
    Is a "child" object of Repo.
    """

    file_path: str
    """Path relative to repo root."""

    is_modified: bool

    auto_run_info: Optional[List[AutoRunBlock]] = None
    """The list of `When/ThenRun` blocks, if present"""

    def __init__(
        self,
        workspace_root: str,
        file_path: str,
        is_modified: bool,
        reader: YamlReader,
    ):
        self.workspace_root = workspace_root
        self.file_path = file_path
        self.is_modified = is_modified

        conts = reader.load(workspace_root, self.file_path)
        SLOG.info(f"Running auto-tasks for workload: {self.file_path}")

        if "AutoRun" not in conts:
            return

        auto_run = conts["AutoRun"]
        if not isinstance(auto_run, list):
            raise ValueError(f"AutoRun must be a list, instead got type {type(auto_run)}")
        self._validate_auto_run(auto_run)
        auto_run_info = []
        for block in auto_run:
            if "ThenRun" in block:
                then_run = block["ThenRun"]
            else:
                then_run = []
            auto_run_info.append(AutoRunBlock(block["When"], then_run))

        self.auto_run_info = auto_run_info

    def _get_relative_path_from_src_workloads(self):
        relative_path = self.file_path.split("src/workloads/")
        if len(relative_path) != 2:
            raise ValueError(f"Invalid workload path {self.file_path}")
        # Example: "src/genny/src/workloads/directory/nested/Workload.yml" will be converted to
        # "directory/nested/Workload.yml"
        return relative_path[1]

    @property
    def snake_case_base_name(self) -> str:
        # Workload path is workspace_root/src/*/src/workloads/**/*.yml.
        # To create a base name we leave only **/*.yml and convert it to snake case. To preserve
        # legacy names, we omit the first directory that was matched as part of **.
        # Example file_path: "src/genny/src/workloads/directory/nested/MyWorkload.yml".

        # Example relative_path: "directory/nested/MyWorkload.yml".
        relative_path = self._get_relative_path_from_src_workloads()

        # Split the extention away. Example: "directory/nested/MyWorkload".
        relative_path = os.path.splitext(relative_path)[0]

        # Split by directory separatior: ["directory", "nested", "MyWorkload"].
        relative_path = relative_path.split(os.sep)

        if len(relative_path) == 1:
            # Convert workload file name to snake case.
            return self._to_snake_case(relative_path[0])
        else:
            # Omit first directory. Example: ["nested", "MyWorkload"].
            relative_path = relative_path[1:]

            # Convert each part to snake case and join with "_". Example: nested_my_workload
            return "_".join([self._to_snake_case(part) for part in relative_path])

    @property
    def relative_path(self) -> str:
        return self.file_path.replace(self.workspace_root, ".")

    def generate_requested_tasks(self, then_run) -> List[GeneratedTask]:
        """
        :return: tasks requested.
        """
        tasks = []
        if len(then_run) == 0:
            tasks += [GeneratedTask(self.snake_case_base_name, None, None, self)]

        for then_run_block in then_run:
            # Just a sanity check; we check this in _validate_auto_run
            assert len(then_run_block) == 1
            [(bootstrap_key, bootstrap_value)] = then_run_block.items()
            task_name = f"{self.snake_case_base_name}_{self._to_snake_case(bootstrap_value)}"
            tasks.append(GeneratedTask(task_name, bootstrap_key, bootstrap_value, self))

        return tasks

    def all_tasks(self) -> List[GeneratedTask]:
        """
        :return: all possible tasks that have an AutoRun block, irrespective of the current
        build-variant etc.
        """
        if not self.auto_run_info:
            return []

        tasks = []
        for block in self.auto_run_info:
            tasks += self.generate_requested_tasks(block.then_run)

        return self._dedup_task(tasks)

    def variant_tasks(self, build: CurrentBuildInfo) -> List[GeneratedTask]:
        """
        :param build: info about current build
        :return: tasks that we should do given the current build e.g. if we have When/ThenRun info etc.
        """
        if not self.auto_run_info:
            return []

        tasks = []
        for block in self.auto_run_info:
            when = block.when
            then_run = block.then_run
            # All When conditions must be true. We set okay: False if any single one is not true.
            okay = True

            for key, condition in when.items():
                if len(condition) != 1:
                    raise ValueError(
                        f"Need exactly one condition per key in When block."
                        f" Got key ${key} with condition ${condition}."
                    )
                operator, value = list(condition.items())[0]
                if operator == "$eq":
                    acceptable_values = value
                    if not isinstance(acceptable_values, list):
                        acceptable_values = [acceptable_values]
                    if not build.has(key, acceptable_values):
                        okay = False
                elif operator == "$neq":
                    unacceptable_values = value
                    if not isinstance(unacceptable_values, list):
                        unacceptable_values = [unacceptable_values]
                    if build.has(key, unacceptable_values):
                        okay = False
                elif self._is_comparison_operator(operator):
                    if key not in build.conts:
                        okay = False
                    else:
                        build_value = build.conts[key]
                        build_version = self._extract_major_minor_version_tuple(build_value)
                        value_version = self._extract_major_minor_version_tuple(value)
                        if build_version is not None and value_version is not None:
                            build_value = build_version
                            value = value_version
                        if not self._compare(operator, build_value, value):
                            okay = False
                else:
                    raise ValueError(
                        f"The only supported operators are $eq, $neq, $gte, $lte, $gt, $lte. Got ${operator}"
                    )

            if okay:
                tasks += self.generate_requested_tasks(then_run)

        return self._dedup_task(tasks)

    _MAIN_BRANCHES = {"master", "main", "production"}
    _MAX_VERSION = (9999, 9999)

    def _extract_major_minor_version_tuple(self, branch_name):
        """
        Tries to extract major and minor version from branch name.
        Version branch names are formated 'v<major version>.<minor version>'
        Example: v4.0, v5.3, v6.2

        :return: Tuple (major version, minor version) or None if input is not a version branch name
        """
        if not isinstance(branch_name, str):
            return None

        if branch_name in self._MAIN_BRANCHES:
            return self._MAX_VERSION

        match = re.match(r"\Av(\d+).(\d+)\Z", branch_name)
        if match:
            return tuple(int(v) for v in match.group(1, 2))
        else:
            return None

    _COMPARISON_OPERATORS = {"$gt", "$gte", "$lt", "$lte"}

    def _is_comparison_operator(self, operator: str):
        return operator in self._COMPARISON_OPERATORS

    def _compare(self, operator: str, lhs, rhs) -> bool:
        try:
            if operator == "$gt":
                return lhs > rhs
            elif operator == "$gte":
                return lhs >= rhs
            elif operator == "$lt":
                return lhs < rhs
            elif operator == "$lte":
                return lhs <= rhs
        except TypeError as e:
            raise TypeError(
                f"{e}: lhs={lhs}, rhs={rhs}. Workload: {self.relative_path}"
            ).with_traceback(e.__traceback__)
        raise ValueError(
            f"The only supported comparison operators are $gte, $lte, $gt, $lte. Got ${operator}"
        )

    @staticmethod
    def _dedup_task(tasks: List[GeneratedTask]) -> List[GeneratedTask]:
        """
        Evergreen barfs if a task is declared more than once, and the AutoTask impl may add the same task twice to the
        list. For an example, if we have two When blocks that are both true (and no ThenRun task), we will add the base
        task twice. So we need to dedup the final task list.

        :return: unique tasks.
        """
        # Sort the result to make checking dict equality in unittests easier.
        return sorted(list(set([task for task in tasks])))

    @staticmethod
    def _validate_auto_run(auto_run):
        """Perform syntax validation on the auto_run section."""
        if not isinstance(auto_run, list):
            raise ValueError(f"AutoRun must be a list, instead got {type(auto_run)}")
        for block in auto_run:
            if not isinstance(block["When"], dict) or "When" not in block:
                raise ValueError(
                    f"Each AutoRun block must consist of a 'When' and optional 'ThenRun' section,"
                    f" instead got {block}"
                )
            if "ThenRun" in block:
                if not isinstance(block["ThenRun"], list):
                    raise ValueError(
                        f"ThenRun must be of type list. Instead was type {type(block['ThenRun'])}."
                    )
                for then_run_block in block["ThenRun"]:
                    if not isinstance(then_run_block, dict):
                        raise ValueError(
                            f"Each block in ThenRun must be of type dict."
                            f" Instead was type {type(then_run_block)}."
                        )
                    elif len(then_run_block) != 1:
                        raise ValueError(
                            f"Each block in ThenRun must contain one key/value pair. Instead was length"
                            f" {len(then_run_block)}."
                        )

    # noinspection RegExpAnonymousGroup
    @staticmethod
    def _to_snake_case(camel_case):
        """
        Converts CamelCase to snake_case, useful for generating test IDs
        https://stackoverflow.com/questions/1175208/
        :return: snake_case version of camel_case.
        """
        s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", camel_case)
        s2 = re.sub("-", "_", s1)
        return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s2).lower()


class Repo:
    """
    Represents the git checkout.
    """

    def __init__(
        self,
        lister: WorkloadLister,
        reader: YamlReader,
        workspace_root: str,
    ):
        self._modified_repo_files = None
        self.workspace_root = workspace_root
        self.lister = lister
        self.reader = reader
        self._all_workloads = None

    def all_workloads(self) -> List[Workload]:
        if self._all_workloads is None:
            all_files = self.lister.all_workload_files()
            modified = self.lister.modified_workload_files()
            self._all_workloads = [
                Workload(
                    workspace_root=self.workspace_root,
                    file_path=fpath,
                    is_modified=fpath in modified,
                    reader=self.reader,
                )
                for fpath in all_files
            ]
        return self._all_workloads

    def modified_workloads(self) -> List[Workload]:
        return [workload for workload in self.all_workloads() if workload.is_modified]

    def all_tasks(self) -> List[GeneratedTask]:
        """
        :return: All possible tasks from all possible workloads
        """
        # Double list-comprehensions always read backward to me :(
        return [task for workload in self.all_workloads() for task in workload.all_tasks()]

    def variant_tasks(self, build: CurrentBuildInfo) -> List[GeneratedTask]:
        """
        :return: Tasks to schedule given the current variant (runtime)
        """
        return [task for workload in self.all_workloads() for task in workload.variant_tasks(build)]

    def patch_tasks(self, build: CurrentBuildInfo) -> List[GeneratedTask]:
        """
        :return: Tasks for modified workloads current variant (runtime)
        """
        return [
            task for workload in self.modified_workloads() for task in workload.variant_tasks(build)
        ]

    def tasks(self, op: OpName, build: CurrentBuildInfo) -> List[GeneratedTask]:
        """
        :param op: current cli invocation
        :param build: current build info
        :return: tasks that should be scheduled given the above
        """
        if op == OpName.ALL_TASKS:
            tasks = self.all_tasks()
        elif op == OpName.PATCH_TASKS:
            tasks = self.patch_tasks(build)
        elif op == OpName.VARIANT_TASKS:
            tasks = self.variant_tasks(build)
        else:
            raise Exception("Invalid operation mode")
        return tasks

class ConfigWriter:
    """
    Takes tasks and converts them to shrub Configuration objects.
    """

    class FileFormat(enum.Enum):
        JSON = "json"
        YAML = "yaml"

    @staticmethod
    def write_config(execution: int, config: Configuration, output_file: str, file_format: FileFormat) -> None:
        """
        :param config: The configuration to write
        :param output_file: What file to write to.
        """
        success = False
        raised = None
        try:
            if file_format == ConfigWriter.FileFormat.JSON:
                out_text = config.to_json()
            if file_format == ConfigWriter.FileFormat.YAML:
                out_text = config.to_yaml()
            os.makedirs(os.path.dirname(output_file), exist_ok=True)
            if os.path.exists(output_file):
                os.unlink(output_file)
            with open(output_file, "w") as output:
                output.write(out_text)
                SLOG.debug("Wrote task json", output_file=output_file, contents=out_text)
            success = True
        except Exception as e:
            raised = e
            raise e
        finally:
            SLOG.info(
                f"{'Succeeded' if success else 'Failed'} to write to {output_file} from cwd={os.getcwd()}."
                f"{raised if raised else ''}"
            )
            if execution != 0:
                SLOG.warning(
                    "Repeated executions will not re-generate tasks.",
                    execution=execution,
                )

    @staticmethod
    def create_config(
        op: OpName, build: CurrentBuildInfo, tasks: List[GeneratedTask]
    ) -> Configuration:
        """
        Creates a configuration for generated tasks to either execute particular tasks for
        a variant or to define global tasks used by variants.

        :param op: Used to determine whether to write global config or tasks for a variant.
        :param build: Information about the current build. Only used for variant configuration
        :param output_file: What file to write to.
        :return: the configuration object to write (exposed for testing)
        """
        config = Configuration()
        if op != OpName.ALL_TASKS:
            ConfigWriter.configure_variant_tasks(config, tasks, build.variant)
        else:
            ConfigWriter.configure_all_tasks_modern(config, tasks)
        return config

    @staticmethod
    def configure_variant_tasks(
        config: Configuration,
        tasks: List[GeneratedTask],
        variant: str,
        activate_all: Optional[bool] = None,
        activate_tasks: Set[VariantTask] = frozenset(),
    ) -> None:
        all_tasks = []
        for task in tasks:
            if VariantTask(variant, task.name) in activate_tasks:
                should_activate = True
            else:
                should_activate = activate_all
            all_tasks.append(TaskSpec(task.name).activate(should_activate))
        config.variant(variant).tasks(all_tasks)

    @staticmethod
    def configure_all_tasks_modern(config: Configuration, tasks: List[GeneratedTask]) -> None:
        config.exec_timeout(64800)  # 18 hours
        for task in tasks:
            bootstrap = {
                "test_control": task.name,
                "auto_workload_path": task.workload.relative_path,
            }
            if task.bootstrap_key:
                bootstrap[task.bootstrap_key] = task.bootstrap_value

            t = config.task(task.name)
            t.priority(5)
            t.commands(
                [
                    CommandDefinition()
                    .command("timeout.update")
                    .params({"exec_timeout_secs": 86400, "timeout_secs": 86400}),  # 24 hours
                    CommandDefinition().function("f_run_dsi_workload").vars(bootstrap),
                ]
            )


def main(mode_name: str, dry_run: bool, workspace_root: str) -> None:
    reader = YamlReader()
    if dry_run:
        expansions = {
            "build_variant": "dryrun-variant",
            "execution": "0",
            "mongodb_setup": "dryrun-setup",
            "branch_name": "v0.0",
        }
    else:
        try:
            expansions = reader.load(workspace_root, "expansions.yml")
        except FileNotFoundError:
            SLOG.error(
                f"Evergreen expansions file {os.path.join(workspace_root, 'expansions.yml')} does not exist. Ensure this file exists and that it is in the correct location."
            )
            sys.exit(1)
    build = CurrentBuildInfo(expansions)
    op = OpName.from_flag(mode_name)
    workload_file_pattern = os.path.join(workspace_root, "src", "*", "src", "workloads", "**", "*.yml")
    lister = WorkloadLister(workspace_root=workspace_root, workload_file_pattern=workload_file_pattern)
    repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)
    tasks = repo.tasks(op=op, build=build)

    output_file = os.path.join(workspace_root, "build", "TaskJSON", "Tasks.json")
    config = ConfigWriter.create_config(op, build, tasks)
    if dry_run:
        SLOG.debug("Tasks json content", contents=config.to_json)
    else:
        ConfigWriter.write_config(build.execution, config, output_file, ConfigWriter.FileFormat.JSON)
