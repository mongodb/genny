from cmd_runner import run_command
from curator import poplar_grpc


def main_canaries_runner(cleanup_metrics: bool, workspace_root: str, genny_repo_root: str):
    """
    Intended to be the main entry point for running canaries.
    """
    with poplar_grpc(
        cleanup_metrics, workspace_root=workspace_root, genny_repo_root=genny_repo_root
    ):
        run_command(cmd=["genny-canaries"], capture=False, check=True)
