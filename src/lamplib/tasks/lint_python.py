import black
import os


def lint_python(genny_repo_root):
    path = os.path.join(genny_repo_root, "src", "lamplib")
    black.main(["--check", path])
