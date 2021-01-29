import black
import os
import structlog

SLOG = structlog.get_logger(__name__)


def lint_python(genny_repo_root: str, fix: bool = False):
    path = os.path.join(genny_repo_root, "src", "lamplib")
    if not fix:
        cmd = ["--check"]
    else:
        cmd = []
    cmd.append(path)
    SLOG.info("Running black", black_args=cmd)
    black.main(cmd)
