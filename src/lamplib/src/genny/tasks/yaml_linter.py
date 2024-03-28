import os
import os.path as path
import sys
import yaml

import structlog
import yamllint.cli
from genny.tasks.yaml_linter_constants import (
    REQUIRED_FIELDS,
    GRANDFATHERED_WORKLOADS_WITHOUT_KEYWORDS,
)

SLOG = structlog.get_logger(__name__)


def main(genny_repo_root: str, lint_format: bool):
    workload_dirs = [
        path.join(genny_repo_root, "src", "workloads"),
        path.join(genny_repo_root, "src", "phases"),
    ]

    has_errors = False
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
        has_errors = True

    all_have_descriptions = True
    for workload_yaml in workload_yamls:
        if not check_required_fields(workload_yaml):
            all_have_descriptions = False
    if not all_have_descriptions:
        SLOG.error(
            f"""
            The above YAML workloads lack one or more of the required {", ".join(REQUIRED_FIELDS)} fields. 
            These fields should be populated with human-readable values. After doing so, please re-run ./run-genny lint-yaml
            """
        )
        has_errors = True

    if lint_format:
        config_file_path = path.join(genny_repo_root, ".yamllint")
        yamllint_argv = ["--strict", "--config-file", config_file_path] + all_yamls

        SLOG.info(
            "Linting workload YAML files with yamllint",
            count=len(all_yamls),
            yamllint_argv=yamllint_argv,
        )

        yamllint.cli.run(yamllint_argv)

    if has_errors:
        sys.exit(1)


def check_required_fields(yaml_path):
    workload = _load_yaml(yaml_path)
    no_errors = True
    for field in REQUIRED_FIELDS:
        if field not in workload:
            if field == "Keywords" and yaml_path in GRANDFATHERED_WORKLOADS_WITHOUT_KEYWORDS:
                continue
            SLOG.error(f"Genny workload {yaml_path} lacks the {field} field.")
            no_errors = False
    return no_errors


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
