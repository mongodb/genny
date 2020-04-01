import os
import sys
import glob
import subprocess

from shrub.config import Configuration


def _check_output(cwd, *args, **kwargs):
    old_cwd = os.getcwd()
    try:
        os.chdir(cwd)
        out = subprocess.check_output(*args, **kwargs)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e
    finally:
        os.chdir(old_cwd)

    if out.decode() == "":
        return []
    return out.decode().strip().split("\n")


class Repo:
    def __init__(self, repo_root: str):
        self.repo_root = repo_root
        self._modified_repo_files = None

    @staticmethod
    def _normalize_path(filename: str) -> str:
        return filename.split("workloads/", 1)[1]

    def modified_workload_files(self):
        command = (
            "git diff --name-only --diff-filter=AMR "
            # TODO: don't use rtimmons/
            "$(git merge-base HEAD rtimmons/master) -- src/workloads/"
        )
        print(f"Command: {command}")
        lines = _check_output(self.repo_root, command, shell=True)
        return {os.path.join(self.repo_root, line) for line in lines if line.endswith(".yml")}

    def all_workload_files(self):
        pattern = os.path.join(self.repo_root, "src", "workloads", "*", "*.yml")
        return {*glob.glob(pattern)}

    def workloads(self):
        all_files = self.all_workload_files()
        modified = self.modified_workload_files()
        return [Workload(fpath, fpath in modified) for fpath in all_files]


class Runtime:
    def workload_setup(self):
        pass


class GeneratedTask:
    pass


class Workload:
    def __init__(self, file_path: str, is_modified: bool):
        self.file_path = file_path
        self.is_modified = is_modified

    def __repr__(self):
        return f"<{self.file_path},{self.is_modified}>"


class TaskWriter:
    def write(self) -> Configuration:
        raise NotImplementedError()


class CLI:
    def __init__(self, cwd=None):
        self.cwd = cwd if cwd else os.getcwd()
        self.repo = Repo(cwd)

    def main(self, argv=None):
        argv = argv if argv else sys.argv
        print(self.repo.workloads())

    def all_tasks(self):
        pass


if __name__ == "__main__":
    cli = CLI("/Users/rtimmons/Projects/genny")
    cli.main()
