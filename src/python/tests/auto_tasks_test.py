import json
import unittest
from typing import NamedTuple, Dict, List, Tuple, Optional
from unittest.mock import MagicMock

from shrub.config import Configuration

from gennylib.auto_tasks import Runtime, CLIOperation, DirectoryStructure, Repo, ConfigWriter, \
    Reader


class Example(NamedTuple):
    given_expansions: Dict[str, str]
    given_args: List[str]
    expect_output: Tuple[str, dict]


class MockFile(NamedTuple):
    base_name: str
    modified: bool
    yaml_conts: Optional[dict]

    @property
    def repo_path(self):
        return "src/workloads/" + self.base_name


class MockReader(Reader):
    def __init__(self, files: List[MockFile]):
        self.files = files

    def load(self, path):
        found = [f for f in self.files if f.repo_path == path or f.base_name == path]
        if not found:
            raise Exception(f"Couldn't find {path} in {[self.files]}")
        return found[0].yaml_conts


class AutoTasksTests(unittest.TestCase):
    @staticmethod
    def helper(files: List[MockFile],
               argv: List[str]) -> Configuration:
        # Create "dumb" mocks.
        lister: DirectoryStructure = MagicMock(name="lister",
                                               spec=DirectoryStructure,
                                               instance=True)
        runtime: Runtime = MagicMock(name="runtime",
                                     spec=Runtime,
                                     instance=True)
        reader: Reader = MockReader(files)

        # Make them smarter.
        lister.all_workload_files.return_value = [v.repo_path for v in files]
        lister.modified_workload_files.return_value = [v.repo_path for v in files if v.modified]

        # Put a dummy argv[0] as test sugar
        argv.insert(0, "dummy.py")

        # And send them off into the world.
        op = CLIOperation.parse(argv, reader)
        repo = Repo(lister, reader)
        tasks = repo.tasks(op, runtime)
        writer = ConfigWriter(op)

        config = writer.write(tasks, write=False)
        return config

    def test_all_tasks(self):
        config = self.helper([
            MockFile("scale/Foo.yml", False, {})
        ], ["all_tasks"])
        parsed = json.loads(config.to_json())
        self.assertEqual(["foo"], [task["name"] for task in parsed["tasks"]])
        self.assertDictEqual({
            "func": "f_run_dsi_workload",
            "vars": {
                "test_control": "foo",
                "auto_workload_path": "scale/Foo.yml",
            },
        }, parsed["tasks"][0]["commands"][0])

    def test_variant_tasks(self):
        config = self.helper([
            MockFile("expansions.yml", False, {
                "build_variant": "some-build-variant",
                "mongodb_setup": "some-setup",
            }),
            MockFile("src/Foo.yml", False, {
                "AutoRun": {
                    "Requires": {
                        "mongodb_setup": ["some-setup"]
                    }
                }
            }),
        ], ["variant_tasks"])

        parsed = json.loads(config.to_json())
        self.assertEqual(parsed,{'buildvariants': [{'name': 'some-build-variant', 'tasks': [{'name': 'foo'}]}]})

    def test_patch_tasks(self):
        config = self.helper([
            MockFile("expansions.yml", False, {
                "build_variant": "some-build-variant",
                "mongodb_setup": "some-setup",
            }),
            MockFile("src/Foo.yml", True, {
                "AutoRun": {
                    "Requires": {
                        "mongodb_setup": ["some-other-setup"]
                    }
                }
            }),
            MockFile("src/Bar.yml", False, {
                "AutoRun": {
                    "Requires": {
                        "mongodb_setup": ["some-other-setup"]
                    }
                }
            }),
        ], ["patch_tasks"])
        parsed = json.loads(config.to_json())
        self.assertEqual(parsed, {
            'buildvariants': [{'name': 'some-build-variant', 'tasks': [{'name': 'foo'}]}]})