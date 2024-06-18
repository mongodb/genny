from os import path
import os
from pathlib import PurePosixPath
from typing import NamedTuple, Optional

from jinja2 import Environment, PackageLoader
import structlog
import yaml

from tasks.mothra_service import MothraService

SLOG = structlog.get_logger(__name__)


class Workload(NamedTuple):
    name: str
    owner: str
    description: str
    keywords: list[str]
    path: str
    github_link: str
    support_channel_id: Optional[str] = None
    support_channel_name: Optional[str] = None


class DocumentationGenerator:
    def __init__(self, genny_repo_root: str):
        self.genny_repo_root = genny_repo_root
        self.mothra_service = MothraService(genny_repo_root)

    def generate(self):
        SLOG.info("Generating workload documentation.")
        input_dir = path.join(self.genny_repo_root, "src", "workloads")
        output_file = path.join(
            self.genny_repo_root, "docs", "generated", "workloads.md"
        )
        self._generate_workload_documentation(input_dir, output_file, "workload")

        SLOG.info("Generating phase documentation.")
        input_dir = path.join(self.genny_repo_root, "src", "phases")
        output_file = path.join(self.genny_repo_root, "docs", "generated", "phases.md")
        self._generate_workload_documentation(input_dir, output_file, "phase")

    def _generate_workload_documentation(
        self, input_dir, output_file, documentation_type
    ):
        workload_dirs = [input_dir]
        SLOG.info("Gathering workloads from files.", workload_dirs=workload_dirs)

        workloads = [
            self._get_workload_from_file(yaml_path)
            for yaml_path in self._get_workload_files(workload_dirs)
        ]
        # Sort workloads by path to ensure consistent ordering.
        workloads.sort(key=lambda workload: workload.path)

        SLOG.info("Workloads found.", count=len(workloads))

        SLOG.info("Generating documentation.")
        documentation = self._generate_markdown(workloads, documentation_type)

        SLOG.info("Writing documentation to file.")
        self._write_documentation(documentation, output_file)

    def _get_workload_files(self, roots: list[str]) -> list[str]:
        yaml_files = []
        for root in roots:
            for dirpath, _, filenames in os.walk(root):
                for filename in filenames:
                    if filename.endswith(".yml"):
                        yaml_files.append(path.join(dirpath, filename))
        return yaml_files

    def _get_workload_from_file(self, yaml_path: str) -> Workload:
        with open(yaml_path, "r") as f:
            workload_yaml = yaml.safe_load(f)
            workload_name = PurePosixPath(yaml_path).stem
            path = yaml_path.replace(self.genny_repo_root, "")
            team = self.mothra_service.get_team(workload_yaml.get("Owner"))

            return Workload(
                name=workload_name,
                owner=workload_yaml.get("Owner"),
                description=workload_yaml.get("Description"),
                keywords=workload_yaml.get("Keywords", []),
                path=path,
                github_link=f"https://www.github.com/mongodb/genny/blob/master{path}",
                support_channel_id=team.support_slack_channel_id if team else None,
                support_channel_name=team.support_slack_channel_name if team else None,
            )

    def _generate_markdown(self, workloads: list[Workload], documentation_type) -> str:
        environment = Environment(loader=PackageLoader("genny"), autoescape=False)
        template = environment.get_template("documentation.md.j2")
        return template.render(
            workloads=workloads, documentation_type=documentation_type
        )

    def _write_documentation(self, documentation: str, output_file: str):
        with open(output_file, "w") as f:
            f.write(documentation)
