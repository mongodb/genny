import json
import shutil
import unittest
import os
from typing import NamedTuple, List, Optional
from unittest.mock import MagicMock
from unittest.mock import patch, mock_open
from shrub.config import Configuration
import structlog

from genny.tasks.auto_tasks import (
    CurrentBuildInfo,
    OpName,
    WorkloadLister,
    Repo,
    ConfigWriter,
    YamlReader,
)

SLOG = structlog.get_logger(__name__)


class MockFile(NamedTuple):
    base_name: str
    modified: bool
    yaml_conts: Optional[dict]


class MockReader(YamlReader):
    def __init__(self, files: List[MockFile]):
        self.files = files

    def load(self, workspace_root: str, path: str):
        if not self.exists(path):
            raise Exception(f"Yaml file {path} not configured.")
        return self._find_file(path).yaml_conts

    def exists(self, path: str) -> bool:
        found = self._find_file(path)
        return found is not None

    def _find_file(self, path: str):
        found = [f for f in self.files if f.base_name == path]
        if found:
            return found[0]
        return None


class BaseTestClass(unittest.TestCase):
    def setUp(self):
        self.workspace_root = "."

    def cleanUp(self):
        shutil.rmtree(self.workspace_root)

    def assert_result(
        self, given_files: List[MockFile], and_mode: str, then_writes: dict, to_file: str
    ):
        # Create "dumb" mocks.
        lister: WorkloadLister = MagicMock(name="lister", spec=WorkloadLister, instance=True)
        reader: YamlReader = MockReader(given_files)
        # Make them smarter.
        lister.all_workload_files.return_value = [
            v.base_name
            for v in given_files
            # Hack: Prevent calling e.g. expansions.yml a 'workload' file
            # (solution would be to pass in workload files as a different
            # param to this function, but meh)
            if "/" in v.base_name
        ]
        lister.modified_workload_files.return_value = [
            v.base_name for v in given_files if v.modified and "/" in v.base_name
        ]

        # And send them off into the world.
        expansions = reader.load(self.workspace_root, "expansions.yml")
        build = CurrentBuildInfo(expansions)
        op = OpName.from_flag(and_mode)
        repo = Repo(lister, reader, workspace_root=self.workspace_root)
        tasks = repo.tasks(op, build)

        config = ConfigWriter.create_config(op, build, tasks)
        parsed = json.loads(config.to_json())
        try:
            self.assertDictEqual(then_writes, parsed)
        except AssertionError:
            print(parsed)
            raise


TIMEOUT_COMMAND = {
    "command": "timeout.update",
    "params": {"exec_timeout_secs": 86400, "timeout_secs": 86400},
}


def expansions_mock(exp_vars) -> MockFile:
    yaml_conts = {"build_variant": "some-build-variant", "execution": "0"}
    yaml_conts.update(exp_vars)
    return MockFile(
        base_name="expansions.yml",
        modified=False,
        yaml_conts=yaml_conts,
    )


class AutoTasksTests(BaseTestClass):
    def test_all_tasks(self):
        expansions = expansions_mock({"mongodb_setup": "matches"})
        empty_genny_unmodified = MockFile(
            base_name="src/genny/src/workloads/scale/EmptyUnmodified.yml",
            modified=False,
            yaml_conts={},
        )
        empty_other_unmodified = MockFile(
            base_name="src/other/src/workloads/scale/EmptyUnmodifiedOther.yml",
            modified=False,
            yaml_conts={},
        )
        auto_run = {
            "AutoRun": [
                {
                    "When": {"mongodb_setup": {"$eq": "matches"}},
                    "ThenRun": [{"mongodb_setup": "a"}, {"arb_bootstrap_key": "b"}],
                }
            ]
        }
        multi_modified_genny = MockFile(
            base_name="src/genny/src/workloads/src/Multi.yml",
            modified=True,
            yaml_conts=auto_run,
        )
        multi_modified_other = MockFile(
            base_name="src/other/src/workloads/src/MultiOther.yml",
            modified=True,
            yaml_conts=auto_run,
        )
        nested_unmodified_genny = MockFile(
            base_name="src/genny/src/workloads/directory/nested_directory/Unmodified.yml",
            modified=False,
            yaml_conts=auto_run,
        )
        nested_unmodified_other = MockFile(
            base_name="src/other/src/workloads/directory/nested_directory/UnmodifiedOther.yml",
            modified=False,
            yaml_conts=auto_run,
        )

        self.assert_result(
            given_files=[
                expansions,
                empty_genny_unmodified,
                empty_other_unmodified,
                multi_modified_genny,
                multi_modified_other,
                nested_unmodified_genny,
                nested_unmodified_other,
            ],
            and_mode="all_tasks",
            then_writes={
                "exec_timeout_secs": 64800,
                "tasks": [
                    {
                        "name": "multi_a",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "multi_a",
                                    "auto_workload_path": "src/genny/src/workloads/src/Multi.yml",
                                    "mongodb_setup": "a",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "multi_b",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "multi_b",
                                    "auto_workload_path": "src/genny/src/workloads/src/Multi.yml",
                                    "arb_bootstrap_key": "b",  # test that arb bootstrap_key works
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "multi_other_a",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "multi_other_a",
                                    "auto_workload_path": "src/other/src/workloads/src/MultiOther.yml",
                                    "mongodb_setup": "a",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "multi_other_b",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "multi_other_b",
                                    "auto_workload_path": "src/other/src/workloads/src/MultiOther.yml",
                                    "arb_bootstrap_key": "b",  # test that arb bootstrap_key works
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "nested_directory_unmodified_a",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "nested_directory_unmodified_a",
                                    "auto_workload_path": "src/genny/src/workloads/directory/nested_directory/Unmodified.yml",
                                    "mongodb_setup": "a",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "nested_directory_unmodified_b",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "nested_directory_unmodified_b",
                                    "auto_workload_path": "src/genny/src/workloads/directory/nested_directory/Unmodified.yml",
                                    "arb_bootstrap_key": "b",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "nested_directory_unmodified_other_a",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "nested_directory_unmodified_other_a",
                                    "auto_workload_path": "src/other/src/workloads/directory/nested_directory/UnmodifiedOther.yml",
                                    "mongodb_setup": "a",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                    {
                        "name": "nested_directory_unmodified_other_b",
                        "commands": [
                            TIMEOUT_COMMAND,
                            {
                                "func": "f_run_dsi_workload",
                                "vars": {
                                    "test_control": "nested_directory_unmodified_other_b",
                                    "auto_workload_path": "src/other/src/workloads/directory/nested_directory/UnmodifiedOther.yml",
                                    "arb_bootstrap_key": "b",
                                },
                            },
                        ],
                        "priority": 5,
                    },
                ],
            },
            to_file="./build/TaskJSON/Tasks.json",
        )

    def run_test_variant_tasks(self, given_files, then_writes_tasks):
        then_writes = {"buildvariants": [{"name": "some-build-variant"}]}
        then_writes["buildvariants"][0].update(then_writes_tasks)
        self.assert_result(
            given_files=given_files,
            and_mode="variant_tasks",
            then_writes=then_writes,
            to_file="./build/TaskJSON/Tasks.json",
        )

    def test_variant_tasks_1(self):
        """
        Basic test.

        Tests (true) $eq and $neq with scalar values.
        Tests When with single (true) condition.
        Tests multiple When/ThenRun blocks.
        """
        expansions = expansions_mock({"mongodb_setup": "matches"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/MultiUnmodified.yml",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {"mongodb_setup": {"$eq": "matches"}},
                            "ThenRun": [{"mongodb_setup": "c"}, {"mongodb_setup": "d"}],
                        }
                    ]
                },
            ),
            MockFile(
                base_name="src/workloads/scale/Foo.yml",
                modified=False,
                yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$neq": "not-matches"}}}]},
            ),
        ]
        then_writes_tasks = {
            "tasks": [
                {"name": "multi_unmodified_c"},
                {"name": "multi_unmodified_d"},
                {"name": "foo"},
            ]
        }
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_variant_tasks_2(self):
        """Similar to above test.

        Tests (false) $eq and $neq with scalar values.
        Tests When with single (false) condition.
        Tests multiple When/ThenRun blocks.
        """
        expansions = expansions_mock({"mongodb_setup": "matches"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/MultiUnmodified.yml",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "mongodb_setup": {"$eq": "not-matches"}  # this invalidates the When
                            },
                            "ThenRun": [{"mongodb_setup": "c"}, {"mongodb_setup": "d"}],
                        }
                    ]
                },
            ),
            MockFile(
                base_name="src/workloads/scale/Foo.yml",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "mongodb_setup": {"$neq": "matches"}  # this invalidates the When
                            }
                        }
                    ]
                },
            ),
        ]
        then_writes_tasks = {}
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_variant_tasks_3(self):
        """
        More complex test.

        Tests (true) $eq and $neq conditions with lists of values.
        Tests When with multiple (true) conditions.
        Tests multiple When/ThenRun blocks.
        """
        expansions = expansions_mock({"mongodb_setup": "matches", "branch_name": "v4.4"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/MultiUnmodified.yml",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "mongodb_setup": {"$neq": ["something-else", "something-else-1"]},
                                "branch_name": {"$neq": ["v4.0", "v4.2"]},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "c"},
                                {"mongodb_setup": "d"},
                                {"infrastructure_provisioning": "infra_a"},
                            ],
                        },
                        {
                            "When": {"mongodb_setup": {"$eq": ["matches", "matches1", "matches2"]}},
                            "ThenRun": [{"mongodb_setup": "e"}, {"mongodb_setup": "f"}],
                        },
                    ]
                },
            ),
            MockFile(
                base_name="src/workloads/scale/Foo.yml",
                modified=False,
                yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$neq": "not-matches"}}}]},
            ),
        ]
        then_writes_tasks = {
            "tasks": [
                {"name": "multi_unmodified_c"},
                {"name": "multi_unmodified_d"},
                {"name": "multi_unmodified_e"},
                {"name": "multi_unmodified_f"},
                {"name": "multi_unmodified_infra_a"},
                {"name": "foo"},
            ],
        }
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_variant_tasks_4(self):
        """
        Similar to above test.

        Tests (false) $eq and $neq conditions with lists of values.
        Tests When with multiple (one true, one false) conditions.
        Tests multiple When/ThenRun blocks.
        """
        expansions = expansions_mock({"mongodb_setup": "matches", "branch_name": "v4.2"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/MultiUnmodified.yml",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "mongodb_setup": {"$neq": ["something-else", "something-else-2"]},
                                "branch_name": {
                                    "$neq": ["v4.0", "v4.2"]
                                },  # this invalidates the When
                            },
                            "ThenRun": [
                                {"mongodb_setup": "c"},
                                {"mongodb_setup": "d"},
                                {"infrastructure_provisioning": "infra_a"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {
                                    "$eq": [
                                        "not-matches",
                                        "not-matches1",
                                    ]  # this invalidates the When
                                }
                            },
                            "ThenRun": [{"mongodb_setup": "e"}, {"mongodb_setup": "f"}],
                        },
                    ]
                },
            ),
        ]
        then_writes_tasks = {}
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_variant_tasks_5(self):
        """
        Test comparison expressions and custom comparison for versions
        """
        expansions = expansions_mock({"mongodb_setup": "matches-1", "branch_name": "v4.2"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/CompareBranchName",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "branch_name": {"$gt": "v4.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gt_greater"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gt": "v4.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gt_equal"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v4.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_greater"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v4.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_equal"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v4.3"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_less"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v3.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_greater_major"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v5.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_less_major"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lt": "v4.3"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_less"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lt": "v4.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_equal"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v4.3"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_less"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v4.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_equal"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v4.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_greater"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v3.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_greater_major"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v5.2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_less_major"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lt": "v10.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_two_digits_major"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lt": "v4.10"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_two_digits_minor"},
                            ],
                        },
                    ]
                },
            ),
            MockFile(
                base_name="src/workloads/src/CompareMongodbSetup",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "mongodb_setup": {"$gt": "matches-0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gt_greater"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$gt": "matches-1"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gt_equal"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$gte": "matches-0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_greater"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$gte": "matches-1"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_equal"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$gte": "matches-2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte_less"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$lt": "matches-2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_less"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$lt": "matches-1"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt_equal"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$lte": "matches-2"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_less"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$lte": "matches-1"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_equal"},
                            ],
                        },
                        {
                            "When": {
                                "mongodb_setup": {"$lte": "matches-0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte_greater"},
                            ],
                        },
                    ]
                },
            ),
        ]
        then_writes_tasks = {
            "tasks": [
                {"name": "compare_branch_name_gt_greater"},
                {"name": "compare_branch_name_gte_equal"},
                {"name": "compare_branch_name_gte_greater"},
                {"name": "compare_branch_name_gte_greater_major"},
                {"name": "compare_branch_name_lt_less"},
                {"name": "compare_branch_name_lt_two_digits_major"},
                {"name": "compare_branch_name_lt_two_digits_minor"},
                {"name": "compare_branch_name_lte_equal"},
                {"name": "compare_branch_name_lte_less"},
                {"name": "compare_branch_name_lte_less_major"},
                {"name": "compare_mongodb_setup_gt_greater"},
                {"name": "compare_mongodb_setup_gte_equal"},
                {"name": "compare_mongodb_setup_gte_greater"},
                {"name": "compare_mongodb_setup_lt_less"},
                {"name": "compare_mongodb_setup_lte_equal"},
                {"name": "compare_mongodb_setup_lte_less"},
            ]
        }
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_variant_tasks_6(self):
        """
        Test custom comparison for main branches
        """
        expansions = expansions_mock({"mongodb_setup": "matches", "branch_name": "main"})
        given_files = [
            expansions,
            MockFile(
                base_name="src/workloads/src/CompareBranchName",
                modified=False,
                yaml_conts={
                    "AutoRun": [
                        {
                            "When": {
                                "branch_name": {"$gt": "v6.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gt"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$gte": "v6.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "gte"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lt": "v6.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lt"},
                            ],
                        },
                        {
                            "When": {
                                "branch_name": {"$lte": "v6.0"},
                            },
                            "ThenRun": [
                                {"mongodb_setup": "lte"},
                            ],
                        },
                    ]
                },
            ),
        ]
        then_writes_tasks = {
            "tasks": [
                {"name": "compare_branch_name_gt"},
                {"name": "compare_branch_name_gte"},
            ]
        }
        self.run_test_variant_tasks(given_files=given_files, then_writes_tasks=then_writes_tasks)

    def test_patch_tasks(self):
        """patch_tasks is just variant_tasks for only modified files."""

        expansions = expansions_mock({"mongodb_setup": "matches"})
        multi_yaml_consts = {
            "AutoRun": [
                {
                    "When": {"mongodb_setup": {"$neq": ["something-else", "something-else-2"]}},
                    "ThenRun": [
                        {"mongodb_setup": "c"},
                        {"mongodb_setup": "d"},
                        {
                            "infrastructure_provisioning": "infra_a"
                        },  # test that arb bootstrap_key works
                    ],
                },
                {
                    "When": {"mongodb_setup": {"$eq": ["matches", "matches", "matches2"]}},
                    "ThenRun": [
                        {"mongodb_setup": "e"},
                        {"mongodb_setup": "f"},
                    ],
                },
            ]
        }
        multi_modified = MockFile(
            base_name="src/workloads/src/MultiModified.yml",
            modified=True,
            yaml_conts=multi_yaml_consts,
        )
        multi_unmodified = MockFile(
            base_name="src/workloads/src/MultiUnmodified.yml",
            modified=False,
            yaml_conts=multi_yaml_consts,
        )
        matches_unmodified = MockFile(
            base_name="src/workloads/scale/MatchesUnmodified.yml",
            modified=False,
            yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$eq": "matches"}}}]},
        )
        matches_modified = MockFile(
            base_name="src/workloads/scale/MatchesModified.yml",
            modified=True,
            yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$eq": "matches"}}}]},
        )
        nested_unmodified = MockFile(
            base_name="src/workloads/directory/nested/Unmodified.yml",
            modified=False,
            yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$eq": "matches"}}}]},
        )
        nested_modified = MockFile(
            base_name="src/workloads/directory/nested/Modified.yml",
            modified=True,
            yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$eq": "matches"}}}]},
        )
        self.assert_result(
            given_files=[
                expansions,
                multi_modified,
                multi_unmodified,
                matches_unmodified,
                matches_modified,
                nested_unmodified,
                nested_modified,
            ],
            and_mode="patch_tasks",
            then_writes={
                "buildvariants": [
                    {
                        "name": "some-build-variant",
                        "tasks": [
                            {"name": "multi_modified_c"},
                            {"name": "multi_modified_d"},
                            {"name": "multi_modified_e"},
                            {"name": "multi_modified_f"},
                            {"name": "multi_modified_infra_a"},
                            {"name": "matches_modified"},
                            {"name": "nested_modified"},
                        ],
                    }
                ]
            },
            to_file="./build/TaskJSON/Tasks.json",
        )

    def test_later_execution(self):
        """Ensures that we exercise the code path warning about task generation not working
        on later executions and don't throw exceptions."""
        with patch("builtins.open", mock_open()) as open_mock, patch(
            "os.makedirs"
        ) as makedirs_mock, patch("os.unlink") as unlink_mock, patch(
            "os.path.exists", return_value=False
        ) as exists_mock:
            config = Configuration()
            ConfigWriter.write_config(execution=5, config=config, output_file="fakefile123", file_format=ConfigWriter.FileFormat.JSON)


def test_dry_run_all_tasks():
    """This is a dry run of schedule_global_auto_tasks."""

    genny_repo_root = os.environ.get("GENNY_REPO_ROOT", None)
    workspace_root = "."
    if not genny_repo_root:
        raise Exception(
            f"GENNY_REPO_ROOT env var {genny_repo_root} either not set or does not exist. "
            f"This is set when you run through the 'run-genny' wrapper"
        )
    try:
        build = CurrentBuildInfo({"build_variant": "Test Variant", "execution": 1})
        op = OpName.from_flag("all_tasks")
        workload_file_pattern = os.path.join(workspace_root, "src", "*", "src", "workloads", "**", "*.yml")
        lister = WorkloadLister(workspace_root=workspace_root, workload_file_pattern=workload_file_pattern)
        reader = YamlReader()
        repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)
        tasks = repo.tasks(op=op, build=build)
        os.path.join(workspace_root, "build", "TaskJSON", "Tasks.json")

        ConfigWriter.create_config(op, build, tasks)
    except Exception as e:
        SLOG.error(
            "'./run-genny auto-tasks --tasks all_tasks' is failing. "
            "This is likely a user error in the auto_tasks syntax. "
            "Refer to error msg for what needs to be fixed."
        )
        raise e


if __name__ == "__main__":
    unittest.main()
