import os

from genny import cmd_runner


def run_generate_uuid_tag(genny_repo_root: str):
    path = os.path.join(
        genny_repo_root, "src", "lamplib", "src", "genny", "tasks", "generate-uuid-tag.sh"
    )
    cmd_runner.run_command(
        cmd=[path], cwd=genny_repo_root, capture=False, check=True,
    )
