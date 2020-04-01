import os
import sys
import glob
from collections import abc
from typing import List, Optional

from shrub.config import Configuration


class Repo:
    def __init__(self, cwd: str):
        self.cwd = cwd

    def is_file_modified(self, file_path):
        return True


class Runtime:
    def workload_setup(self):
        pass


class GeneratedTask:
    pass


class Workload:
    def __init__(self, file_path: str, repo: Repo):
        self.file_path = file_path
        self.repo = repo

    def is_modified(self) -> bool:
        return self.repo.is_file_modified(self.file_path)

    @staticmethod
    def list(genny_root: str, repo: Repo) -> List['Workload']:
        path = os.path.join(genny_root, "src", "workloads", "*", "*.yml")
        return [Workload(fpath,repo) for fpath in glob.glob(path)]

class TaskWriter(abc):
    def write(self) -> Configuration:
        raise NotImplementedError()


class CLI:
    def __init__(self, cwd=None):
        self.cwd = cwd if cwd else os.getcwd()
        self.repo = Repo(cwd)

    def main(self, argv=None):
        argv = argv if argv else sys.argv

    def all_tasks(self):
