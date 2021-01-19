import os
import shutil
import subprocess
from typing import List


def run_resmoke(genny_repo_root: str, resmoke_args: List[str]) -> List[str]:
    mongo_repo_path: str = os.path.join(genny_repo_root, "build", "resmoke-mongo")
    resmoke_venv: str = os.path.join(mongo_repo_path, "resmoke_venv")
    resmoke_python: str = os.path.join(resmoke_venv, "bin", "python3")

    # ./src/genny/../mongo = ./src/mongo exists so use it
    if (
        os.path.exists(os.path.join(genny_repo_root, "..", "mongo"))
        and os.getcwd() != genny_repo_root
    ):
        mongo_repo_path = os.path.join(genny_repo_root, "..", "mongo")

    # Clone repo unless exists
    if not os.path.exists(mongo_repo_path):
        res = subprocess.run(["git", "clone", "git@github.com:mongodb/mongo.git", mongo_repo_path])
        res.check_returncode()

        # See comment in evergreen.yml - mongodb_archive_url
        res = subprocess.run(
            ["git", "checkout", "298d4d6bbb9980b74bded06241067fe6771bef68"], cwd=mongo_repo_path
        )
        res.check_returncode()

    # Setup resmoke venv unless exists
    resmoke_setup_sentinel = os.path.join(resmoke_venv, "setup-done")
    if not os.path.exists(resmoke_setup_sentinel):
        shutil.rmtree(resmoke_venv, ignore_errors=True)
        import venv

        venv.create(env_dir=resmoke_venv, with_pip=True, symlinks=True)
        reqs_file = os.path.join(mongo_repo_path, "etc", "pip", "evgtest-requirements.txt")
        cmd = [resmoke_python, "-mpip", "install", "-r", reqs_file]
        res = subprocess.run(cmd)
        res.check_returncode()
        open(resmoke_setup_sentinel, "w")

    return [
        resmoke_python,
        os.path.join(mongo_repo_path, "buildscripts", "resmoke.py"),
        *resmoke_args,
    ]
