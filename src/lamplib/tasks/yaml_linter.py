import structlog
import os
import os.path as path
import sys

import yamllint.cli

SLOG = structlog.get_logger(__name__)


def main():

    if not path.exists(".genny-root"):
        SLOG.error("Please run this script from the root of the Genny repository", cwd=os.getcwd())
        sys.exit(1)

    yaml_dirs = [
        path.join(os.getcwd(), "src", "workloads"),
        path.join(os.getcwd(), "src", "phases"),
        path.join(os.getcwd(), "src", "resmokeconfig"),
    ]

    yaml_files = [path.join(os.getcwd(), "evergreen.yml")]

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
        sys.exit(1)

    config_file_path = path.join(os.getcwd(), ".yamllint")

    yamllint_argv = ["--strict", "--config-file", config_file_path] + yaml_files

    print("Linting {} Genny workload YAML files with yamllint".format(len(yaml_files)))

    yamllint.cli.run(yamllint_argv)
