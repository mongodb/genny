import os
import os.path as path
import sys

import structlog
import yamllint.cli

SLOG = structlog.get_logger(__name__)


def main(genny_repo_root: str):
    yaml_dirs = [
        path.join(genny_repo_root, "src", "workloads"),
        path.join(genny_repo_root, "src", "phases"),
        path.join(genny_repo_root, "src", "resmokeconfig"),
    ]

    yaml_files = [path.join(genny_repo_root, "evergreen.yml")]

    has_error = False

    for yaml_dir in yaml_dirs:
        for dirpath, dirnames, filenames in os.walk(yaml_dir):
            for filename in filenames:
                if filename.endswith(".yaml"):
                    SLOG.error("All YAML files should have the .yml extension", found=filename)
                    # Don't error immediately so all violations can be printed with one run
                    # of this script.
                    has_error = True
                elif filename.endswith(".yml"):
                    yaml_files.append(path.join(dirpath, filename))

    if has_error:
        sys.exit(1)

    if len(yaml_files) == 0:
        SLOG.error("Did not find any YAML files to lint", in_dirs=yaml_dirs)
        raise Exception("No yamls found")

    config_file_path = path.join(os.getcwd(), ".yamllint")

    yamllint_argv = ["--strict", "--config-file", config_file_path] + yaml_files

    SLOG.info(
        "Linting workload YAML files with yamllint",
        count=len(yaml_files),
        yamllint_argv=yamllint_argv,
    )

    yamllint.cli.run(yamllint_argv)
