"""
                                All Tasks Legacy

- Invoked as

        genny/genny/scripts/genny_auto_tasks.sh \
            --generate-all-tasks \
            --output build/all_tasks.json
        # Key: --generate-all-tasks
        cat ../src/genny/genny/build/all_tasks.json

- cwd           `${workdir}/src`
- genny root    `${workdir}/src/genny/genny`                        == `./genny/genny`
- output file   `${workdir}/src/genny/genny/build/all_tasks.json`   == `./genny/genny/build/all_tasks.json`
- Does not rely on any environment yaml files
- Does not need variant info


                            Variant Patch Tasks Legacy

- Invoked as

        genny/genny/scripts/genny_auto_tasks.sh \
            --output build/patch_tasks.json \
            --variants "${build_variant}" --modified
        # key: --modified
        cat genny/genny/build/patch_tasks.json

- cwd           `${workdir}/src`
- genny root    `${workdir}/src/genny/genny`                        == `./genny/genny`
- output file   `${workdir}/src/genny/genny/build/patch_tasks.json` == `./genny/genny/build/patch_tasks.json`
- Does not rely on any environment yaml files
- Passes in variant through `--variants` flag.


                        Variant Tasks Legacy

- Invoked as

        ../src/genny/genny/scripts/genny_auto_tasks.sh \
            --output build/auto_tasks.json --variants "${build_variant}" \
            --autorun
        # key: --autorun
        cat ../src/genny/genny/build/auto_tasks.json

- cwd            `${workdir}/work`
- genny root     `${workdir}/src/genny/genny`                       == `../src/genny/genny`
- output file    `${workdir}/src/genny/genny/build/auto_tasks.json` == `../src/genny/genny/build/auto_tasks.json`
- Passes in variant through `--variants` command-line


                    All Tasks Modern

- Invoked as

        ${workdir}/src/genny/genny_all_tasks.sh

- cwd           `${workdir}`
- genny root    `${workdir}/src/genny`                  == `./src/genny`
- output file   `${workdir}/run/build/Tasks/Tasks.json  == `./run/build/Tasks/Tasks.json`
- Does not rely on any environment yaml files

                    Variant Patch Tasks Modern

- Invoked as

        ${workdir}/src/genny/genny_patch_tasks.sh

- cwd           `${workdir}`
- genny root    `${workdir}/src/genny`                  == `./src/genny`
- output file   `${workdir}/run/build/Tasks/Tasks.json  == `./run/build/Tasks/Tasks.json`
- Relies on     `${workdir}/expansions.yml`
- Gets variant from expansions.yml


                    Variant Tasks Modern

- Invoked as

        ${workdir}/src/genny/genny_variant_tasks.sh

- cwd           `${workdir}`
- genny root    `${workdir}/src/genny`                  == `./src/genny`
- output file   `${workdir}/run/build/Tasks/Tasks.json  == `./run/build/Tasks/Tasks.json`
- Relies on     `${workdir}/expansions.yml`
- Gets variant from expansions.yml


                    Summary

- If we have `--generate-all-tasks`

    - cwd            `${workdir}/work`
    - genny root     `${workdir}/src/genny/genny`                       == `../src/genny/genny`
    - output file    `${workdir}/src/genny/genny/build/auto_tasks.json` == `../src/genny/genny/build/auto_tasks.json`
    - Passes in variant through `--variants` command-line

- If we have `--modified`

    - cwd           `${workdir}/src`
    - genny root    `${workdir}/src/genny/genny`                        == `./genny/genny`
    - output file   `${workdir}/src/genny/genny/build/patch_tasks.json` == `./genny/genny/build/patch_tasks.json`
    - Get variant from `--variants` flag.

- If we have `--autorun`

    - cwd            `${workdir}/work`
    - genny root     `${workdir}/src/genny/genny`                       == `../src/genny/genny`
    - output file    `${workdir}/src/genny/genny/build/auto_tasks.json` == `../src/genny/genny/build/auto_tasks.json`
    - Get variant from `--variants` flag.

- Else we're the "modern" style.

    - cwd           `${workdir}`
    - genny root    `${workdir}/src/genny`                  == `./src/genny`
    - output file   `${workdir}/run/build/Tasks/Tasks.json  == `./run/build/Tasks/Tasks.json`
    - Relies on     `${workdir}/expansions.yml`
    - Gets variant from expansions.yml

    Assert we have `./expansions.yml`

"""

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


def _to_snake_case(camel_case):
    """
    Converts CamelCase to snake_case, useful for generating test IDs
    https://stackoverflow.com/questions/1175208/
    :return: snake_case version of camel_case.
    """
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", camel_case)
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

    def modified_workload_files(self) -> Set[str]:
        command = (
            "git diff --name-only --diff-filter=AMR "
            # TODO: don't use rtimmons/
            "$(git merge-base HEAD rtimmons/master) -- src/workloads/"
        )
        print(f"Command: {command}")
        lines = _check_output(self.repo_root, command, shell=True)
        return {os.path.join(self.repo_root, line) for line in lines if line.endswith(".yml")}

    def all_workload_files(self) -> Set[str]:
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

    def all_tasks(self) -> List["GeneratedTask"]:
        """
        :return: All possible tasks
        """
        return [task for workload in self.all_workloads() for task in workload.all_tasks()]

    def variant_tasks(self, runtime: "Runtime"):
        """
        :return: Tasks to schedule given the current variant (runtime)
        """
        return [
            task for workload in self.all_workloads() for task in workload.variant_tasks(runtime)
        ]

    def patch_tasks(self) -> List["GeneratedTask"]:
        """
        :return: Tasks for modified workloads current variant (runtime)
        """
        return [task for workload in self.modified_workloads() for task in workload.all_tasks()]

    def tasks(self, op: "CLIOperation", runtime: "Runtime") -> List["GeneratedTask"]:
        if op.mode == OpName.ALL_TASKS:
            tasks = self.all_tasks()
        elif op.mode == OpName.PATCH_TASKS:
            tasks = self.patch_tasks()
        elif op.mode == OpName.VARIANT_TASKS:
            tasks = self.variant_tasks(runtime)
        else:
            raise Exception("Invalid operation mode")
        return tasks


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

    def file_base_name(self) -> str:
        return str(os.path.basename(self.file_path).split(".yml")[0], encoding="utf-8")

    def relative_path(self) -> str:
        return self.file_path.split("src/workloads/")[1]

    def all_tasks(self) -> List[GeneratedTask]:
        base = _to_snake_case(self.file_base_name())
        if self.setups is None:
            return [GeneratedTask(base, None, self)]
        return [
            GeneratedTask(f"{base}_{_to_snake_case(setup)}", setup, self) for setup in self.setups
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


class OpName(enum.Enum):
    ALL_TASKS = object()
    VARIANT_TASKS = object()
    PATCH_TASKS = object()


class ConfigWriter:
    def __init__(self, op: "CLIOperation"):
        self.op = op

    def write(self, tasks: List[GeneratedTask]) -> Configuration:
        if self.op.mode != OpName.ALL_TASKS:
            config: Configuration = self.variant_tasks(tasks, self.op.variant)
        else:
            config = (
                self.all_tasks_legacy(tasks) if self.op.is_legacy else self.all_tasks_modern(tasks)
            )
        return config

    def variant_tasks(self, tasks: List[GeneratedTask], variant: str) -> Configuration:
        c = Configuration()
        c.variant(variant).tasks([TaskSpec(task.name) for task in tasks])
        return c

    def all_tasks_legacy(self, tasks: List[GeneratedTask]) -> Configuration:
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

    def all_tasks_modern(self, tasks: List[GeneratedTask]) -> Configuration:
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


class CLIOperation(NamedTuple):
    mode: OpName
    variant: Optional[str]
    is_legacy: bool
    output_file: str
    repo_root: str  # TODO

    @staticmethod
    def parse(argv: List[str]) -> "CLIOperation":
        out = CLIOperation(OpName.ALL_TASKS, None, False, "")
        if "--generate-all-tasks" in argv:
            out.mode = OpName.ALL_TASKS
            out.is_legacy = True
        if "--modified" in argv:
            out.mode = OpName.PATCH_TASKS
            out.is_legacy = True
        if "--autorun" in argv:
            out.mode = OpName.VARIANT_TASKS
            out.is_legacy = True
        if out.mode in {OpName.PATCH_TASKS, OpName.VARIANT_TASKS}:
            variant = argv.index("--variant")
            out.variant = argv[variant + 1]
        if "--output" in argv:
            out.output_file = argv[argv.index("--output") + 1]

        if not out.is_legacy:
            out.output_file = "./run/build/Tasks/Tasks.json"
            if argv[1] == "all_tasks":
                out.mode = OpName.ALL_TASKS
            if argv[1] == "patch_tasks":
                out.mode = OpName.PATCH_TASKS
            if argv[1] == "variant_tasks":
                out.mode = OpName.VARIANT_TASKS
            with open("expansions.yml") as exp:
                parsed = yaml.safe_load(exp)
                out.variant = parsed["build_variant"]
        return out


# def write(path: str, conts: Configuration):
#     # with open(path, "w+") as output:
#     #     output.write(conts.to_json())
#     print(f"To {path} >>\n{conts.to_json()}\n\n")


def main(argv: List[str]) -> None:
    op = CLIOperation.parse(argv)
    runtime = Runtime(os.getcwd())

    lister = Lister(op.repo_root)
    repo = Repo(lister)
    tasks = repo.tasks(op, runtime)

    writer = ConfigWriter(op)
    writer.write(tasks)


if __name__ == "__main__":
    main(sys.argv)
