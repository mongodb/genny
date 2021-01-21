import subprocess

from curator import poplar_grpc


def main_canaries_runner(cleanup_metrics: bool):
    """
    Intended to be the main entry point for running canaries.
    """
    with poplar_grpc(cleanup_metrics):
        res = subprocess.run("genny-canaries")
        res.check_returncode()
