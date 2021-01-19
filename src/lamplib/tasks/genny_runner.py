import subprocess
import structlog
import shlex
import os

SLOG = structlog.get_logger(__name__)

from curator import poplar_grpc


def main_genny_runner(args, genny_repo_root, cleanup: bool):
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc(cleanup):
        path = os.path.join(genny_repo_root, "dist", "bin", "genny_core")
        if not os.path.exists(path):
            SLOG.error("genny_core not found. Run compile first.", path=path)
            raise Exception(f"genny_core not found at {path}.")
        cmd = [path, *args]
        SLOG.info("Running genny", command=" ".join(shlex.quote(x) for x in cmd))

        res = subprocess.run(cmd)
        res.check_returncode()
