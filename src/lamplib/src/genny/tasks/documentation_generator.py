from os import path
import os
from pathlib import PurePosixPath
from typing import NamedTuple
from natsort import natsorted

from jinja2 import Environment, PackageLoader
import structlog
import yaml

SLOG = structlog.get_logger(__name__)


class Workload(NamedTuple):
    name: str
    owner: str
    description: str
    keywords: list[str]
    path: str
    github_link: str


def main(genny_repo_root: str):
    SLOG.info("Generating workload documentation.")
    input_dir = path.join(genny_repo_root, "src", "workloads")
    output_file = path.join(genny_repo_root, "docs", "generated", "workloads.md")
    generate_workload_documentation(genny_repo_root, input_dir, output_file, "workload")

    SLOG.info("Generating phase documentation.")
    input_dir = path.join(genny_repo_root, "src", "phases")
    output_file = path.join(genny_repo_root, "docs", "generated", "phases.md")
    generate_workload_documentation(genny_repo_root, input_dir, output_file, "phase")


def generate_workload_documentation(genny_repo_root, input_dir, output_file, documentation_type):
    workload_dirs = [input_dir]
    SLOG.info("Gathering workloads from files.", workload_dirs=workload_dirs)

    workloads = [
        get_workload_from_file(yaml_path, genny_repo_root)
        for yaml_path in get_workload_files(workload_dirs)
    ]
    # Sort workloads by path to ensure consistent ordering.
    workloads = natsorted(workloads, key=lambda workload: workload.path)

    SLOG.info("Workloads found.", count=len(workloads))

    SLOG.info("Generating documentation.")
    documentation = generate_markdown(workloads, documentation_type)

    SLOG.info("Writing documentation to file.")
    write_documentation(documentation, output_file)


def get_workload_files(roots: list[str]) -> list[str]:
    yaml_files = []
    for root in roots:
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                if filename.endswith(".yml"):
                    yaml_files.append(path.join(dirpath, filename))
    return yaml_files


def get_workload_from_file(yaml_path: str, genny_repo_root: str) -> Workload:
    with open(yaml_path, "r") as f:
        workload_yaml = yaml.safe_load(f)
        workload_name = PurePosixPath(yaml_path).stem
        path = yaml_path.replace(genny_repo_root, "")
        return Workload(
            name=workload_name,
            owner=workload_yaml.get("Owner"),
            description=workload_yaml.get("Description"),
            keywords=workload_yaml.get("Keywords", []),
            path=path,
            github_link=f"https://www.github.com/mongodb/genny/blob/master{path}",
        )


def generate_markdown(workloads: list[Workload], documentation_type) -> str:
    environment = Environment(loader=PackageLoader("genny"), autoescape=False)
    template = environment.get_template("documentation.md.j2")
    return template.render(workloads=workloads, documentation_type=documentation_type)


def write_documentation(documentation: str, output_file: str):
    with open(output_file, "w") as f:
        f.write(documentation)
