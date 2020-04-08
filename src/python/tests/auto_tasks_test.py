import json
import unittest
from typing import NamedTuple, Dict, List, Tuple, Optional
from unittest.mock import MagicMock

from shrub.config import Configuration

from gennylib.auto_tasks import (
    CurrentBuildInfo,
    CLIOperation,
    FileLister,
    Repo,
    ConfigWriter,
    YamlReader,
)


class MockFile(NamedTuple):
    base_name: str
    modified: bool
    yaml_conts: Optional[dict]


class MockReader(YamlReader):
    def __init__(self, files: List[MockFile]):
        self.files = files

    def load(self, path: str):
        found = self._find_file(path)
        if not found:
            raise Exception(f"Couldn't find {path} in {[self.files]}")
        return found.yaml_conts

    def exists(self, path: str) -> bool:
        found = self._find_file(path)
        return found is not None

    def _find_file(self, path: str):
        found = [f for f in self.files if f.base_name == path]
        if found:
            return found[0]
        return None


class Scenario(NamedTuple):
    op: CLIOperation
    config: Configuration
    parsed: dict


def w(b: str) -> str:
    return f"src/workloads/{b}"


MULTI_SETUP_FILE_MODIFIED = MockFile(
    base_name=w("src/Multi.yml"),
    modified=True,
    yaml_conts={
        "AutoRun": {
            "Requires": {"mongodb_setup": ["matches"]},
            "PrepareEnvironmentWith": {"mongodb_setup": ["a", "b"]},
        }
    },
)

MULTI_SETUP_FILE_UNMODIFIED = MockFile(
    base_name=w("src/MultiUnmodified.yml"),
    modified=False,
    yaml_conts={
        "AutoRun": {
            "Requires": {"mongodb_setup": ["matches"]},
            "PrepareEnvironmentWith": {"mongodb_setup": ["c", "d"]},
        }
    },
)


def helper(files: List[MockFile], argv: List[str]) -> Scenario:
    # Create "dumb" mocks.
    lister: FileLister = MagicMock(name="lister", spec=FileLister, instance=True)
    reader: YamlReader = MockReader(files)

    # Make them smarter.
    lister.all_workload_files.return_value = [v.base_name for v in files if "/" in v.base_name]
    lister.modified_workload_files.return_value = [
        v.base_name for v in files if v.modified and "/" in v.base_name
    ]

    # Put a dummy argv[0] as test sugar
    argv.insert(0, "dummy.py")

    # And send them off into the world.
    build = CurrentBuildInfo(reader)
    op = CLIOperation.parse(argv, reader)
    repo = Repo(lister, reader)
    tasks = repo.tasks(op, build)
    writer = ConfigWriter(op)

    config = writer.write(tasks, write=False)
    parsed = json.loads(config.to_json())
    return Scenario(op, config, parsed)


class AutoTasksTests(unittest.TestCase):
    def test_all_tasks(self):
        scenario = helper(
            [
                MockFile(base_name=w("scale/Foo.yml"), modified=False, yaml_conts={}),
                MULTI_SETUP_FILE_MODIFIED,
                MockFile(base_name="expansions.yml", modified=False, yaml_conts={}),
            ],
            ["all_tasks"],
        )
        expected = {
            "tasks": [
                {
                    "name": "foo",
                    "commands": [
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {"test_control": "foo", "auto_workload_path": "scale/Foo.yml"},
                        }
                    ],
                    "priority": 5,
                },
                {
                    "name": "multi_a",
                    "commands": [
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "multi_a",
                                "auto_workload_path": "src/Multi.yml",
                                "mongodb_setup": "a",
                            },
                        }
                    ],
                    "priority": 5,
                },
                {
                    "name": "multi_b",
                    "commands": [
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "multi_b",
                                "auto_workload_path": "src/Multi.yml",
                                "mongodb_setup": "b",
                            },
                        }
                    ],
                    "priority": 5,
                },
            ],
            "timeout": 64800,
        }
        self.assertDictEqual(scenario.parsed, expected)

    def test_variant_tasks(self):
        scenario = helper(
            [
                MockFile(
                    base_name="expansions.yml",
                    modified=False,
                    yaml_conts={"build_variant": "some-build-variant", "mongodb_setup": "matches"},
                ),
                MULTI_SETUP_FILE_UNMODIFIED,
                MockFile(
                    base_name=w("scale/Foo.yml"),
                    modified=False,
                    yaml_conts={"AutoRun": {"Requires": {"mongodb_setup": ["matches"]}}},
                ),
            ],
            ["variant_tasks"],
        )

        print(scenario.parsed)
        expected = {
            "buildvariants": [
                {
                    "name": "some-build-variant",
                    "tasks": [
                        {"name": "multi_unmodified_c"},
                        {"name": "multi_unmodified_d"},
                        {"name": "foo"},
                    ],
                }
            ]
        }
        self.assertEqual(scenario.parsed, expected)

    def test_patch_tasks(self):
        scenario = helper(
            [
                MockFile(
                    base_name="expansions.yml",
                    modified=False,
                    yaml_conts={
                        "build_variant": "some-build-variant",
                        "mongodb_setup": "some-setup",
                    },
                ),
                MULTI_SETUP_FILE_MODIFIED,
                MULTI_SETUP_FILE_UNMODIFIED,
                MockFile(
                    base_name=w("scale/Foo.yml"),
                    modified=True,
                    yaml_conts={"AutoRun": {"Requires": {"mongodb_setup": ["some-other-setup"]}}},
                ),
                MockFile(
                    base_name=w("scale/Bar.yml"),
                    modified=False,
                    yaml_conts={"AutoRun": {"Requires": {"mongodb_setup": ["some-other-setup"]}}},
                ),
            ],
            ["patch_tasks"],
        )
        expected = {
            "buildvariants": [
                {
                    "name": "some-build-variant",
                    "tasks": [{"name": "multi_a"}, {"name": "multi_b"}, {"name": "foo"}],
                }
            ]
        }
        self.assertEqual(
            scenario.parsed,
            expected,
            "Patch tasks always run for the selected variant "
            "even if their Requires blocks don't align",
        )


class LegacyHappyCaseTests(unittest.TestCase):
    def test_all_tasks(self):
        scenario = helper(
            [
                MockFile(base_name="expansions.yml", modified=False, yaml_conts={}),
                MockFile(base_name=w("src/Foo.yml"), modified=True, yaml_conts={}),
                MockFile(base_name=w("src/Bar.yml"), modified=False, yaml_conts={}),
                MULTI_SETUP_FILE_MODIFIED,
            ],
            ["--generate-all-tasks", "--output", "build/all_tasks.json"],
        )
        expected = {
            "tasks": [
                {
                    "name": "foo",
                    "commands": [
                        {
                            "func": "prepare environment",
                            "vars": {"test": "foo", "auto_workload_path": "src/Foo.yml"},
                        },
                        {"func": "deploy cluster"},
                        {"func": "run test"},
                        {"func": "analyze"},
                    ],
                    "priority": 5,
                },
                {
                    "name": "bar",
                    "commands": [
                        {
                            "func": "prepare environment",
                            "vars": {"test": "bar", "auto_workload_path": "src/Bar.yml"},
                        },
                        {"func": "deploy cluster"},
                        {"func": "run test"},
                        {"func": "analyze"},
                    ],
                    "priority": 5,
                },
                {
                    "name": "multi_a",
                    "commands": [
                        {
                            "func": "prepare environment",
                            "vars": {
                                "test": "multi_a",
                                "auto_workload_path": "src/Multi.yml",
                                "setup": "a",
                            },
                        },
                        {"func": "deploy cluster"},
                        {"func": "run test"},
                        {"func": "analyze"},
                    ],
                    "priority": 5,
                },
                {
                    "name": "multi_b",
                    "commands": [
                        {
                            "func": "prepare environment",
                            "vars": {
                                "test": "multi_b",
                                "auto_workload_path": "src/Multi.yml",
                                "setup": "b",
                            },
                        },
                        {"func": "deploy cluster"},
                        {"func": "run test"},
                        {"func": "analyze"},
                    ],
                    "priority": 5,
                },
            ],
            "timeout": 64800,
        }
        self.assertDictEqual(expected, scenario.parsed)

    def test_variant_tasks(self):
        scenario = helper(
            [
                MockFile(
                    base_name="bootstrap.yml",
                    modified=False,
                    yaml_conts={"mongodb_setup": "my-setup"},
                ),
                MockFile(
                    base_name="src/Foo.yml",
                    modified=False,
                    yaml_conts={"AutoRun": {"Requires": {"mongodb_setup": ["my-setup"]}}},
                ),
                MockFile(
                    base_name="src/Bar.yml",
                    modified=True,
                    yaml_conts={"AutoRun": {"Requires": {"mongodb_setup": ["some-other-setup"]}}},
                ),
            ],
            ["--variants", "some-variant", "--autorun", "--output", "build/all_tasks.json"],
        )
        expected = {"buildvariants": [{"name": "some-variant", "tasks": [{"name": "foo"}]}]}
        self.assertDictEqual(expected, scenario.parsed)


if __name__ == "__main__":
    unittest.main()
