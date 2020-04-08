import enum
import glob
import os
import re
import subprocess
import sys
from typing import NamedTuple, List, Optional, Set

import yaml
from shrub.command import CommandDefinition
from shrub.config import Configuration
from shrub.variant import TaskSpec


def _check_output(cwd, *args, **kwargs):
    old_cwd = os.getcwd()
    try:
        if not os.path.exists(cwd):
            raise Exception(f"Cannot chdir to {cwd} from cwd={os.getcwd()}")
        os.chdir(cwd)
        out = subprocess.check_output(*args, **kwargs)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e
    finally:
        os.chdir(old_cwd)

    if out.decode() == "":
        return []
    return out.decode().strip().split("\n")


# TODO: combine Reader and Runtime?
class Reader:
    def load(self, path):
        if not os.path.exists(path):
            raise Exception(f"No {path} in cwd={os.getcwd()}")
        with open(path) as exp:
            return yaml.safe_load(exp)

    def load_set(self, files: List[str]) -> dict:
        out = dict()
        for file in files:
            if not os.path.exists(file):
                continue
            basename = os.path.basename(file).split(".yml")[0]
            with open(file) as contents:
                out[basename] = yaml.safe_load(contents)
        return out


class DirectoryStructure:
    def __init__(self, repo_root: str, reader: Reader):
        self.repo_root = repo_root
        self._expansions = None
        self.reader = reader

    def all_workload_files(self) -> Set[str]:
        pattern = os.path.join(self.repo_root, "src", "workloads", "*", "*.yml")
        return {*glob.glob(pattern)}

    def modified_workload_files(self) -> Set[str]:
        command = (
            "git diff --name-only --diff-filter=AMR "
            # TODO: don't use rtimmons/
            "$(git merge-base HEAD rtimmons/master) -- src/workloads/"
        )
        # print(f"Command: {command}")
        lines = _check_output(self.repo_root, command, shell=True)
        return {os.path.join(self.repo_root, line) for line in lines if line.endswith(".yml")}


class OpName(enum.Enum):
    ALL_TASKS = object()
    VARIANT_TASKS = object()
    PATCH_TASKS = object()


class CLIOperation(NamedTuple):
    mode: OpName
    variant: Optional[str]
    is_legacy: bool
    output_file_suffix: str

    @property
    def repo_root(self) -> str:
        if self.is_legacy:
            if self.mode != OpName.VARIANT_TASKS:
                return "./genny/genny"
            return "../src/genny/genny"
        return "./src/genny"

    @property
    def output_file(self) -> str:
        if self.is_legacy:
            return os.path.join(self.repo_root, self.output_file_suffix)
        return self.output_file_suffix

    @staticmethod
    def parse(argv: List[str], reader: Reader) -> "CLIOperation":
        mode = OpName.ALL_TASKS
        is_legacy = False
        variant = None
        output_file = None

        if "--generate-all-tasks" in argv:
            mode = OpName.ALL_TASKS
            is_legacy = True
        if "--modified" in argv:
            mode = OpName.PATCH_TASKS
            is_legacy = True
        if "--autorun" in argv:
            mode = OpName.VARIANT_TASKS
            is_legacy = True
        if mode in {OpName.PATCH_TASKS, OpName.VARIANT_TASKS}:
            variant = argv.index("--variants")
            variant = argv[variant + 1]
        if "--output" in argv:
            output_file = argv[argv.index("--output") + 1]
        if is_legacy is False:
            output_file = "./run/build/Tasks/Tasks.json"
            if argv[1] == "all_tasks":
                mode = OpName.ALL_TASKS
            if argv[1] == "patch_tasks":
                mode = OpName.PATCH_TASKS
            if argv[1] == "variant_tasks":
                mode = OpName.VARIANT_TASKS
                variant = reader.load("expansions.yml")["build_variant"]
        return CLIOperation(mode, variant, is_legacy, output_file)


class Runtime:
    use_expansions_yml: bool = False

    def __init__(self, cwd: str,  reader: Reader):
        self.cwd = cwd

        conts = reader.load_set(
            [os.path.join(self.cwd, f"{b}.yml") for b in {"bootstrap", "runtime", "expansions"}]
        )
        if "expansions" in conts:
            self.use_expansions_yml = True
            conts = conts["expansions"]
        else:
            if "bootstrap" not in conts:
                raise Exception(
                    f"Must have either expansions.yml or bootstrap.yml in cwd={self.cwd}"
                )
            bootstrap: dict = conts["bootstrap"]
            runtime: dict = conts["runtime"]
            bootstrap.update(runtime)
            conts = bootstrap
        self.conts = conts

    def has(self, key: str, acceptable_values: List[str]) -> bool:
        if key not in self.conts:
            raise Exception(f"Unknown key {key}. Know about {self.conts.keys()}")
        actual = self.conts[key]
        return any(actual == acceptable_value for acceptable_value in acceptable_values)


class GeneratedTask(NamedTuple):
    name: str
    mongodb_setup: Optional[str]
    workload: "Workload"


class Workload:
    file_path: str
    is_modified: bool
    requires: Optional[dict] = None
    setups: Optional[List[str]] = None

    def __init__(self, file_path: str, is_modified: bool, reader: Reader):
        self.file_path = file_path
        self.is_modified = is_modified

        conts = reader.load(self.file_path)

        if "AutoRun" not in conts:
            return

        auto_run = conts["AutoRun"]
        self.requires = auto_run["Requires"]
        if "PrepareEnvironmentWith" in auto_run:
            prep = auto_run["PrepareEnvironmentWith"]
            if len(prep) != 1 or "mongodb_setup" not in prep:
                raise ValueError(
                    f"Need exactly mongodb_setup: [list] "
                    f"in PrepareEnvironmentWith for file {file_path}"
                )
            self.setups = prep["mongodb_setup"]

    def file_base_name(self) -> str:
        return str(os.path.basename(self.file_path).split(".yml")[0])

    def relative_path(self) -> str:
        return self.file_path.split("src/workloads/")[1]

    def all_tasks(self) -> List[GeneratedTask]:
        base = self._to_snake_case(self.file_base_name())
        if self.setups is None:
            return [GeneratedTask(base, None, self)]
        return [
            GeneratedTask(f"{base}_{self._to_snake_case(setup)}", setup, self)
            for setup in self.setups
        ]

    def variant_tasks(self, runtime: Runtime) -> List[GeneratedTask]:
        if not self.requires:
            return []
        return [
            task
            for task in self.all_tasks()
            if all(
                runtime.has(key, acceptable_values)
                for key, acceptable_values in self.requires.items()
            )
        ]

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
    def __init__(self, lister: DirectoryStructure, reader: Reader):
        self._modified_repo_files = None
        self.lister = lister
        self.reader = reader

    def all_workloads(self) -> List[Workload]:
        all_files = self.lister.all_workload_files()
        modified = self.lister.modified_workload_files()
        return [Workload(fpath, fpath in modified, self.reader) for fpath in all_files]

    def modified_workloads(self) -> List[Workload]:
        return [workload for workload in self.all_workloads() if workload.is_modified]

    def all_tasks(self) -> List[GeneratedTask]:
        """
        :return: All possible tasks
        """
        return [task for workload in self.all_workloads() for task in workload.all_tasks()]

    def variant_tasks(self, runtime: Runtime):
        """
        :return: Tasks to schedule given the current variant (runtime)
        """
        return [
            task for workload in self.all_workloads() for task in workload.variant_tasks(runtime)
        ]

    def patch_tasks(self) -> List[GeneratedTask]:
        """
        :return: Tasks for modified workloads current variant (runtime)
        """
        return [task for workload in self.modified_workloads() for task in workload.all_tasks()]

    def tasks(self, op: CLIOperation, runtime: Runtime) -> List[GeneratedTask]:
        if op.mode == OpName.ALL_TASKS:
            tasks = self.all_tasks()
        elif op.mode == OpName.PATCH_TASKS:
            tasks = self.patch_tasks()
        elif op.mode == OpName.VARIANT_TASKS:
            tasks = self.variant_tasks(runtime)
        else:
            raise Exception("Invalid operation mode")
        return tasks


class ConfigWriter:
    def __init__(self, op: CLIOperation):
        self.op = op

    def write(self, tasks: List[GeneratedTask], write: bool = True) -> Configuration:
        if self.op.mode != OpName.ALL_TASKS:
            config: Configuration = self.variant_tasks(tasks, self.op.variant)
        else:
            config = (
                self.all_tasks_legacy(tasks) if self.op.is_legacy else self.all_tasks_modern(tasks)
            )
        if write:
            try:
                os.makedirs(os.path.dirname(self.op.output_file), exist_ok=True)
                with open(self.op.output_file, "w") as output:
                    output.write(config.to_json())
            except OSError as e:
                print(f"Tried to write to {self.op.output_file} from cwd={os.getcwd()}")
                raise e
        return config

    @staticmethod
    def variant_tasks(tasks: List[GeneratedTask], variant: str) -> Configuration:
        c = Configuration()
        c.variant(variant).tasks([TaskSpec(task.name) for task in tasks])
        return c

    @staticmethod
    def all_tasks_legacy(tasks: List[GeneratedTask]) -> Configuration:
        c = Configuration()
        c.exec_timeout(64800)  # 18 hours
        for task in tasks:
            prep_vars = {"test": task.name, "auto_workload_path": task.workload.relative_path()}
            if task.mongodb_setup:
                prep_vars["setup"] = task.mongodb_setup

            t = c.task(task.name)
            t.priority(5)
            t.commands(
                [
                    CommandDefinition().function("prepare environment").vars(prep_vars),
                    CommandDefinition().function("deploy cluster"),
                    CommandDefinition().function("run test"),
                    CommandDefinition().function("analyze"),
                ]
            )
        return c

    @staticmethod
    def all_tasks_modern(tasks: List[GeneratedTask]) -> Configuration:
        c = Configuration()
        c.exec_timeout(64800)  # 18 hours
        for task in tasks:
            bootstrap = {
                "test_control": task.name,
                "auto_workload_path": task.workload.relative_path(),
            }
            if task.mongodb_setup:
                bootstrap["mongodb_setup"] = task.mongodb_setup

            t = c.task(task.name)
            t.priority(5)
            t.commands([CommandDefinition().function("f_run_dsi_workload").vars(bootstrap)])
        return c


def main(argv: List[str] = None) -> None:
    if not argv:
        argv = sys.argv
    reader = Reader()
    runtime = Runtime(os.getcwd(), reader)
    op = CLIOperation.parse(argv, reader)
    lister = DirectoryStructure(op.repo_root, reader)
    repo = Repo(lister, reader)
    tasks = repo.tasks(op, runtime)

    writer = ConfigWriter(op)
    writer.write(tasks)


if __name__ == "__main__":
    main()
