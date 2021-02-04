import os
from genny import cmd_runner


def run_create_new_actor(genny_repo_root: str, actor_name: str):
    path = os.path.join(genny_repo_root, "src", "lamplib", "tasks", "create-new-actor.sh")
    cmd_runner.run_command(
        cmd=[path, actor_name], cwd=genny_repo_root, capture=False, check=True,
    )
