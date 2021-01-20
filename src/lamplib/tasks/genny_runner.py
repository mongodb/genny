import subprocess
from typing import List

import structlog
import shlex
import os

from curator import poplar_grpc

SLOG = structlog.get_logger(__name__)


def main_genny_runner(genny_args: List[str], genny_repo_root: str):
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc(cleanup=False):
        path = os.path.join(genny_repo_root, "dist", "bin", "genny_core")
        if not os.path.exists(path):
            SLOG.error("genny_core not found. Run install first.", path=path)
            raise Exception(f"genny_core not found at {path}.")
        cmd = [path, *genny_args]
        SLOG.info("Running genny", command=" ".join(shlex.quote(x) for x in cmd))

        res = subprocess.run(cmd)
        res.check_returncode()
