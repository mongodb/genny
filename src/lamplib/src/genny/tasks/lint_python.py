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
    SLOG.debug("Running black", black_args=cmd)

    try:
        black.main(cmd)
    except SystemExit as e:
        if e.code == 0:
            return
        msg = "There were python formatting errors."
        if not fix:
            msg += "  Run the command with the --fix option to fix."
        else:
            msg += "  Some errors may have been fixed. Re-run to verify."
        SLOG.critical(msg)
        raise
