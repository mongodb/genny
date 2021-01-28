import black
import os
import structlog

SLOG = structlog.get_logger(__name__)


def lint_python(genny_repo_root):
    path = os.path.join(genny_repo_root, "src", "lamplib")
    args = ["--check", path]
    SLOG.info("Running black", args=args)
    black.main(args)
