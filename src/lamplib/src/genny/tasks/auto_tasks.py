"""
Generates evergreen tasks based on the current state of the repo.
"""

import glob
import os
import re
from typing import NamedTuple, List, Optional, Set, Dict, Any
import yaml
import structlog

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
        if not os.path.exists(joined):
            raise Exception(f"File {joined} not found.")
        with open(joined) as handle:
            return yaml.safe_load(handle)

    # Really just here for easy mocking.
    def exists(self, path: str) -> bool:
        return os.path.exists(path)

    def load_set(self, workspace_root: str, files: List[str]) -> dict:
        """
        :param workspace_root:
            effective cwd
        :param files:
            files to load relative to cwd
        :return:
            Key the basename (no extension) of the file and value the loaded contents.
            E.g. load_set("expansions") => {"expansions": {"contents":["of","expansions.yml"]}}
        """
        out = dict()
        for to_load in [f for f in files if self.exists(f)]:
            basename = str(os.path.basename(to_load).split(".yml")[0])
            out[basename] = self.load(workspace_root=workspace_root, path=to_load)
        return out


class WorkloadLister:
    """
    Lists files in the repo dir etc.
    Separate from the Repo class for easier testing.
    """

    def __init__(
        self,
        workspace_root: str,
        genny_repo_root: str,
        reader: YamlReader,
        workload_root: Optional[str] = None,
    ):
        self.workspace_root = workspace_root
        self.genny_repo_root = genny_repo_root
        self._expansions = None
        self.reader = reader
        self.workload_root = workload_root

    def all_workload_files(self) -> Set[str]:
        if self.workload_root is None:
            pattern = os.path.join(
                self.workspace_root, "src", "*", "src", "workloads", "**", "*.yml"
            )
        else:
            pattern = os.path.join(self.workload_root, "src", "workloads", "**", "*.yml")
        return {*glob.glob(pattern)}


class CLIOperation(NamedTuple):
    """
    Represents the "input" to what we're doing
    """

    genny_repo_root: str
    workspace_root: str
    workload_root: str
    variant: Optional[str] = None
    execution: Optional[str] = None


class GeneratedTask(NamedTuple):
    name: str
    bootstrap_key: Optional[str]
    bootstrap_value: Optional[str]
    workload: "Workload"


class AutoRunBlock(NamedTuple):
    when: dict
    then_run: dict


class DsiTask(NamedTuple):
    """Represents a  DSI Task entry."""
    name: str
    runs_on_variants: List[str]
    bootstrap_vars: Dict[str, str]


class Workload:
    """
    Represents a workload yaml file.
    Is a "child" object of Repo.
    """

    file_path: str
    """Path relative to repo root."""

    auto_run_info: Optional[List[AutoRunBlock]] = None
    """The list of `When/ThenRun` blocks, if present"""

    def __init__(self, workspace_root: str, file_path: str, reader: YamlReader):
        self.workspace_root = workspace_root
        self.file_path = file_path

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

    @property
    def file_base_name(self) -> str:
        return str(os.path.basename(self.file_path).split(".yml")[0])

    @property
    def snake_case_base_name(self) -> str:
        return self._to_snake_case(self.file_base_name)

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
        :return: all possible tasks irrespective of the current build-variant etc.
        """
        if not self.auto_run_info:
            return [GeneratedTask(self.snake_case_base_name, None, None, self)]

        tasks = []
        for block in self.auto_run_info:
            tasks += self.generate_requested_tasks(block.then_run)

        return self._dedup_task(tasks)

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

    def __init__(self, lister: WorkloadLister, reader: YamlReader, workspace_root: str):
        self._modified_repo_files = None
        self.workspace_root = workspace_root
        self.lister = lister
        self.reader = reader

    def all_workloads(self) -> List[Workload]:
        all_files = self.lister.all_workload_files()
        return [
            Workload(workspace_root=self.workspace_root, file_path=fpath, reader=self.reader,)
            for fpath in all_files
        ]

    def tasks(self, op: CLIOperation) -> List[GeneratedTask]:
        """
        :param op: current cli invocation
        :param build: current build info
        :return: tasks that should be scheduled given the above
        """

        # Double list-comprehensions always read backward to me :(
        return [task for workload in self.all_workloads() for task in workload.all_tasks()]

    def generate_dsi_tasks(self, op: CLIOperation) -> List[DsiTask]:
        """
        Generates list DSI Task objects that represents the schema that DSI expects to generate Evergreen Task file.
        """
        dsi_tasks = []
        tasks = self.tasks(op)
        for task in tasks:
            variants = []
            if task.workload.auto_run_info is None:
                continue
            for block in task.workload.auto_run_info:
                when = block.when
                for key, condition in when.items():
                    if "$eq" in condition and key == "mongodb_setup":
                        acceptable_values = condition["$eq"]
                        if isinstance(acceptable_values, list):
                            variants.extend(acceptable_values)
                        else:
                            variants.append(acceptable_values)
            bootstrap_vars = {
                "test_control": task.name,
                "auto_workload_path": task.workload.relative_path,
            }
            if task.bootstrap_key:
                bootstrap_vars[task.bootstrap_key] = task.bootstrap_value
            dsi_tasks.append(
                DsiTask(name=task.name, runs_on_variants=variants, bootstrap_vars=bootstrap_vars)
            )
        return dsi_tasks


class ConfigWriter:
    def __init__(self, op: CLIOperation):
        self.op = op

    def write(self, tasks: List[DsiTask], write: bool = True) -> None:
        """
        :param tasks: tasks to write
        :param write: boolean to actually write the file - exposed for testing
        :return: the configuration object to write (exposed for testing)
        """
        output_file_name = "DsiTasks.yml"
        if self.op.workload_root is not None:
            repo_name = self.op.workload_root.split("/")[-1]
            output_file_name = f"DsiTasks-{repo_name}.yml"
        output_file = os.path.join(self.op.workspace_root, "build", "DSITasks", output_file_name)

        success = False
        raised = None
        if write:
            try:
                out_text = yaml.dump(self._get_dsi_task_dict(tasks), indent=4, sort_keys=False)
                os.makedirs(os.path.dirname(output_file), exist_ok=True)
                if os.path.exists(output_file):
                    os.unlink(output_file)
                with open(output_file, "w") as output:
                    output.write(out_text)
                    SLOG.debug("Wrote task yaml", output_file=output_file, contents=out_text)
                success = True
            except Exception as e:
                raised = e
                raise e
            finally:
                SLOG.info(
                    f"{'Succeeded' if success else 'Failed'} to write to {output_file} from cwd={os.getcwd()}."
                    f"{raised if raised else ''}"
                )
                if self.op.execution != 0:
                    SLOG.warning(
                        "Repeated executions will not re-generate tasks.",
                        execution=self.op.execution,
                    )

    def _get_dsi_task_dict(self, tasks: List[DsiTask]) -> List[Dict[str, Any]]:
        """Converts DSITask object to a dict to facilitate YAML dump."""
        all_tasks = []
        for task in tasks:
            all_tasks.append(dict(task._asdict()))
        return all_tasks


def main(genny_repo_root: str, workspace_root: str, workload_root: Optional[str] = None) -> None:
    reader = YamlReader()
    op = CLIOperation(
        genny_repo_root=genny_repo_root, workspace_root=workspace_root, workload_root=workload_root,
    )
    lister = WorkloadLister(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        reader=reader,
        workload_root=workload_root,
    )
    repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)
    tasks = repo.generate_dsi_tasks(op=op)

    writer = ConfigWriter(op)
    writer.write(tasks)
