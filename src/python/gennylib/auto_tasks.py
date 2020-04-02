import os
import sys
import glob
import subprocess
from abc import ABC
from typing import NamedTuple, List, Optional, Tuple
import yaml
import re

from shrub.config import Configuration
from shrub.command import CommandDefinition


def to_snake_case(str):
    """
    Converts str to snake_case, useful for generating test id's
    From: https://stackoverflow.com/questions/1175208/elegant-python-function-to-convert-camelcase-to-snake-case
    :return: snake_case version of str.
    """
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", str)
    s2 = re.sub("-", "_", s1)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s2).lower()


def _check_output(cwd, *args, **kwargs):
    old_cwd = os.getcwd()
    try:
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


class Lister:
    def __init__(self, repo_root: str):
        self.repo_root = repo_root

    @staticmethod
    def _normalize_path(filename: str) -> str:
        return filename.split("workloads/", 1)[1]

    def modified_workload_files(self):
        command = (
            "git diff --name-only --diff-filter=AMR "
            # TODO: don't use rtimmons/
            "$(git merge-base HEAD rtimmons/master) -- src/workloads/"
        )
        print(f"Command: {command}")
        lines = _check_output(self.repo_root, command, shell=True)
        return {os.path.join(self.repo_root, line) for line in lines if line.endswith(".yml")}

    def all_workload_files(self):
        pattern = os.path.join(self.repo_root, "src", "workloads", "*", "*.yml")
        return {*glob.glob(pattern)}


class Repo:
    def __init__(self, lister: Lister):
        self._modified_repo_files = None
        self.lister = lister

    def all_workloads(self) -> List["Workload"]:
        all_files = self.lister.all_workload_files()
        modified = self.lister.modified_workload_files()
        return [Workload(fpath, fpath in modified) for fpath in all_files]

    def modified_workloads(self) -> List["Workload"]:
        return [workload for workload in self.all_workloads() if workload.is_modified]


class Runtime:
    def __init__(self, cwd: str, conts: Optional[dict] = None):
        self.conts = conts

    def workload_setup(self):
        pass


class GeneratedTask(NamedTuple):
    name: str
    mongodb_setup: Optional[str]
    workload: "Workload"


class Workload:
    file_path: str
    is_modified: bool
    requires: Optional[dict] = None
    setups: Optional[List[str]] = None

    def __init__(self, file_path: str, is_modified: bool, conts: Optional[dict] = None):
        self.file_path = file_path
        self.is_modified = is_modified

        if not conts:
            with open(file_path) as conts:
                conts = yaml.safe_load(conts)

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

    def file_base_name(self):
        return os.path.basename(self.file_path).split(".yml")[0]

    def relative_path(self):
        return self.file_path.split("src/workloads/")[1]

    # def foo(self):
    #     curr["test"] = "{task_name}_{setup_var}".format(
    #         task_name=curr["test"], setup_var=to_snake_case(setup_var)
    #     )

    def __repr__(self):
        return f"<{self.file_path},{self.is_modified}>"

    def all_tasks(self) -> List[GeneratedTask]:
        base = to_snake_case(self.file_base_name())
        if self.setups is None:
            return [GeneratedTask(base, None, self)]
        return [
            GeneratedTask(f"{base}_{to_snake_case(setup)}", setup, self) for setup in self.setups
        ]

    def variant_tasks(self, runtime: Runtime) -> List[GeneratedTask]:
        if not self.requires:
            return []
        for key, acceptable_values in self.requires.items():
            if not runtime.has(key, acceptable_values):
                return []
        # We know we've met all the "Required" values
        for setup in self.setups:
            pass


class ConfigWriter(ABC):
    def all_tasks(self, tasks: List[GeneratedTask]) -> Configuration:
        raise NotImplementedError()

    def variant_tasks(self, tasks: List[GeneratedTask], variant: Optional[str] = None):
        raise NotImplementedError()


class LegacyConfigWriter(ConfigWriter):
    def all_tasks(self, tasks: List[GeneratedTask]) -> Configuration:
        c = Configuration()
        c.exec_timeout(64800)  # 18 hours
        for task in tasks:
            prep_vars = {
                "test": task.name,
                "auto_workload_path": task.workload.relative_path(),
            }
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

    def variant_tasks(self, tasks: List[GeneratedTask], variant: Optional[str] = None):
        pass


class CLI:
    def __init__(self, cwd: str = None):
        self.cwd = cwd if cwd else os.getcwd()
        self.lister = Lister(self.cwd)
        self.repo = Repo(self.lister)
        self.runtime = Runtime(cwd)

    def main(self, argv=None):
        argv = argv if argv else sys.argv
        tasks = [task for w in self.repo.all_workloads() for task in w.all_tasks()]
        writer = LegacyConfigWriter()
        config = writer.all_tasks(tasks)
        print(config.to_json())

    def all_tasks(self) -> List[GeneratedTask]:
        """
        :return: All possible tasks
        """
        return [task for workload in self.repo.all_workloads() for task in workload.all_tasks()]

    def variant_tasks(self):
        """
        :return: Tasks to schedule given the current variant (runtime)
        """
        return [
            task
            for workload in self.repo.all_workloads()
            for task in workload.variant_tasks(self.runtime)
        ]

    def patch_tasks(self):
        """
        :return: Tasks for modified workloads current variant (runtime)
        """
        return [
            task for workload in self.repo.modified_workloads() for task in workload.all_tasks()
        ]


if __name__ == "__main__":
    cli = CLI("/Users/rtimmons/Projects/genny")
    cli.main()
