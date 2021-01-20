import subprocess

from curator import poplar_grpc


def main_canaries_runner():
    """
    Intended to be the main entry point for running canaries.
    """
    with poplar_grpc(cleanup_metrics=False):
        res = subprocess.run("genny-canaries")
        res.check_returncode()
