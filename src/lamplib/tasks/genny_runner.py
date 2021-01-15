import subprocess
import structlog
import shlex

SLOG = structlog.get_logger(__name__)

from curator import poplar_grpc


def main_genny_runner(args, cleanup=False):
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc(cleanup):
        # TODO: look on PATH and put ./dist/bin or venv onto bin
        cmd = ["./dist/bin/genny_core", *args]
        SLOG.info("Running genny", command=" ".join(shlex.quote(x) for x in cmd))

        res = subprocess.run(cmd)
        res.check_returncode()
