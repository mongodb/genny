import glob
import os
import re
import subprocess
import sys
from abc import ABC
from typing import NamedTuple, List, Optional

import yaml

from shrub.command import CommandDefinition
from shrub.config import Configuration
from shrub.variant import TaskSpec


def to_snake_case(camel_case):
    """
    Converts str to snake_case, useful for generating test id's
    From: https://stackoverflow.com/questions/1175208/elegant-python-function-to-convert-camelcase-to-snake-case
    :return: snake_case version of str.
    """
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", camel_case)
    s2 = re.sub("-", "_", s1)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s2).lower()


def _findup(fpath: str, cwd: str):
    """
    Look "up" the directory tree for fpath starting at cwd.
    Raises if not found.
    :param fpath:
    :param cwd:
    :return:
    """
    curr = cwd
    while os.path.exists(curr):
        if os.path.exists(os.path.join(curr, fpath)):
            return os.path.normpath(curr)
        curr = os.path.join(curr, "..")
    raise BaseException("Cannot find {} in {} or any parent dirs.".format(fpath, cwd))


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


def _yaml_load(files: List[str]) -> dict:
    out = dict()
    for file in files:
        if not os.path.exists(file):
            continue
        basename = os.path.basename(file).split(".yml")[0]
        with open(file) as contents:
            out[basename] = yaml.safe_load(contents)
    return out


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
    use_expansions_yml: bool = False

    def __init__(self, cwd: str, conts: Optional[dict] = None):
        if conts is None:
            conts = _yaml_load(
                [os.path.join(cwd, f"{b}.yml") for b in {"bootstrap", "runtime", "expansions"}]
            )
            if "expansions" in conts:
                self.use_expansions_yml = True
                conts = conts["expansions"]
            else:
                if "bootstrap" not in conts:
                    raise Exception(
                        f"Must have either expansions.yml or bootstrap.yml in cwd={cwd}"
                    )
                bootstrap = conts["bootstrap"]  # type: dict
                runtime = conts["runtime"]  # type: dict
                bootstrap.update(runtime)
                conts = bootstrap
        self.conts = conts

    def has(self, key: str, acceptable_values: List[str]):
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
        return [
            task
            for task in self.all_tasks()
            if all(
                runtime.has(key, acceptable_values)
                for key, acceptable_values in self.requires.items()
            )
        ]


class ConfigWriter(ABC):
    def all_tasks(self, tasks: List[GeneratedTask]) -> Configuration:
        raise NotImplementedError()

    def variant_tasks(self, tasks: List[GeneratedTask], variant: str):
        c = Configuration()
        c.variant(variant).tasks([TaskSpec(task.name) for task in tasks])
        return c


class LegacyConfigWriter(ConfigWriter):
    def all_tasks(self, tasks: List[GeneratedTask]) -> Configuration:
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


class CLI:
    def __init__(self, cwd: str = None):
        self.cwd = cwd if cwd else os.getcwd()
        self.lister = Lister(self.cwd)
        self.repo = Repo(self.lister)
        self.runtime = Runtime(self.cwd)

    def main(self):
        # argv = argv if argv else sys.argv
        # tasks = [task for w in self.repo.all_workloads() for task in w.all_tasks()]
        tasks = self.variant_tasks()
        writer = LegacyConfigWriter()
        config = writer.variant_tasks(tasks, "standalone")
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
    cli = CLI()
    cli.main()
