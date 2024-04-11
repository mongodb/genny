from os import path
import os
from pathlib import Path, PurePosixPath
from re import S
from typing import NamedTuple

from jinja2 import Environment, PackageLoader, select_autoescape
import structlog
import yaml

SLOG = structlog.get_logger(__name__)


class Workload(NamedTuple):
    name: str
    owner: str
    description: str
    keywords: list[str]
    path: str


def main(genny_repo_root: str):
    workload_dirs = [
        path.join(genny_repo_root, "src", "workloads"),
        path.join(genny_repo_root, "src", "phases"),
    ]
    SLOG.info("Gathering workloads from files.", workload_dirs=workload_dirs)

    workloads = [
        get_workload_from_file(yaml_path) for yaml_path in get_workload_files(workload_dirs)
    ]
    # Sort workloads by path to ensure consistent ordering.
    workloads.sort(key=lambda workload: workload.path)

    SLOG.info("Workloads found.", count=len(workloads))

    SLOG.info("Generating documentation.")
    documentation = generate_markdown(workloads)

    SLOG.info("Writing documentation to file.")
    write_documentation(documentation, genny_repo_root)


def get_workload_files(roots: list[str]) -> list[str]:
    yaml_files = []
    for root in roots:
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                if filename.endswith(".yml"):
                    yaml_files.append(path.join(dirpath, filename))
    return yaml_files


def get_workload_from_file(yaml_path: str) -> Workload:
    with open(yaml_path, "r") as f:
        workload_yaml = yaml.safe_load(f)
        workload_name = PurePosixPath(yaml_path).stem
        return Workload(
            name=workload_name,
            owner=workload_yaml.get("Owner"),
            description=workload_yaml.get("Description"),
            keywords=workload_yaml.get("Keywords", []),
            path=f.name,
        )


def generate_markdown(workloads: list[Workload]) -> str:
    environment = Environment(loader=PackageLoader("genny"), autoescape=False)
    template = environment.get_template("documentation.md.j2")
    return template.render(workloads=workloads)


def write_documentation(documentation: str, genny_repo_root: str):
    output_path = path.join(genny_repo_root, "docs", "generated", "workloads.md")
    with open(output_path, "w") as f:
        f.write(documentation)
