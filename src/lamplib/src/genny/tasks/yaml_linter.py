import os
import os.path as path
import sys
import yaml

import structlog
import yamllint.cli

SLOG = structlog.get_logger(__name__)


def main(genny_repo_root: str):
    workload_dirs = [
        path.join(genny_repo_root, "src", "workloads"),
        path.join(genny_repo_root, "src", "phases"),
    ]
    resmoke_dirs = [path.join(genny_repo_root, "src", "resmokeconfig")]
    evergreen_dirs = [path.join(genny_repo_root, "evergreen.yml")]

    workload_yamls, workload_error = _traverse_yamls(workload_dirs)
    resmoke_yamls, resmoke_error = _traverse_yamls(resmoke_dirs)
    evergreen_yamls, evergreen_error = _traverse_yamls(evergreen_dirs)
    all_yamls = workload_yamls + resmoke_yamls + evergreen_yamls

    if workload_error or resmoke_error or evergreen_error:
        SLOG.error(
            "Found invalidly-named yaml files. Please correct and rerun ./run-genny lint-yaml."
        )
        sys.exit(1)

    all_have_descriptions = True
    for workload_yaml in workload_yamls:
        if not check_description(workload_yaml):
            all_have_descriptions = False
    if not all_have_descriptions:
        SLOG.error(
            "The above YAML workloads lack a Description field. This field should be populated with a human-readable description "
            "of the workload and its output metrics. After doing so, please re-run ./run-genny lint-yaml"
        )
        sys.exit(1)

    config_file_path = path.join(os.getcwd(), ".yamllint")
    yamllint_argv = ["--strict", "--config-file", config_file_path] + all_yamls

    SLOG.info(
        "Linting workload YAML files with yamllint",
        count=len(all_yamls),
        yamllint_argv=yamllint_argv,
    )

    yamllint.cli.run(yamllint_argv)


def check_description(yaml_path):
    workload = _load_yaml(yaml_path)
    if "Description" not in workload:
        SLOG.error(f"Genny workload {yaml_path} lacks a Description field.")
        return False
    return True


def _traverse_yamls(roots):
    def check_filename(filename):
        if filename.endswith(".yaml"):
            SLOG.error("All YAML files should have the .yml extension", found=filename)
            return True
        return False

    yaml_files = []
    has_error = False
    for root in roots:
        if os.path.isfile(root):
            return [root], check_filename(root)
        for dirpath, dirnames, filenames in os.walk(root):
            for filename in filenames:
                # Don't error immediately so all violations can be printed with one run
                # of this script.
                has_error = check_filename(filename)
                if filename.endswith(".yml"):
                    yaml_files.append(path.join(dirpath, filename))
    if len(yaml_files) == 0:
        SLOG.error("Did not find any YAML files to lint", in_dirs=directories)
        raise Exception("No yamls found")
    return yaml_files, has_error


def _load_yaml(yaml_path):
    with open(yaml_path) as file:
        workload = yaml.safe_load(file)
        return workload
