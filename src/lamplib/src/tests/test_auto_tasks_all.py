import json
import shutil
import unittest
import os
from typing import NamedTuple, List, Optional
from unittest.mock import MagicMock, mock_open
from unittest.mock import patch
import structlog

from genny.tasks.auto_tasks import (
    CurrentBuildInfo,
    OpName,
    WorkloadLister,
    Repo,
    ConfigWriter,
    YamlReader,
    VariantTask,
)
from genny.tasks.auto_tasks_all import (
    create_configuration,
    get_all_builds,
    parse_activate_generated_tasks,
)
import genny.tasks.auto_tasks_all
from tests.test_auto_tasks import MockFile, MockReader

SLOG = structlog.get_logger(__name__)


class BaseTestClass(unittest.TestCase):
    def setUp(self):
        self.workspace_root = "."

    def cleanUp(self):
        shutil.rmtree(self.workspace_root)

    def assert_result(
        self,
        given_project_file: MockFile,
        and_expansion_file: MockFile,
        and_workload_files: List[MockFile],
        and_mode: str,
        then_writes: dict,
        to_file: str,
    ):
        # Create "dumb" mocks.
        lister: WorkloadLister = MagicMock(name="lister", spec=WorkloadLister, instance=True)
        reader: YamlReader = MockReader(given_files + [given_project_file, and_expansion_file])
        # Make them smarter.
        lister.all_workload_files.return_value = [v.base_name for v in and_workload_files]
        lister.modified_workload_files.return_value = [
            v.base_name for v in and_workload_files if v.modified
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


def make_auto_run(name: str, mongodb_setup: str):
    return MockFile(
        base_name=f"src/genny/src/workloads/src/{name}.yml",
        modified=True,
        yaml_conts={"AutoRun": [{"When": {"mongodb_setup": {"$eq": mongodb_setup}}}]},
    )


class AutoTasksTests(BaseTestClass):
    def test_generate_all(self):
        """
        Tests generating all tasks for a given parsed list of variants in a config file.
        Specifically, tasks a, b, c and c2 correspond to variants/mongodb_setups in the config file,
        while task d does not. This should mean that we generate variants that run a, b, c, and c2
        but no variant should run d.
        """
        EXPECTED = {
            "tasks": [
                {
                    "name": "task_a",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_a",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskA.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
                {
                    "name": "task_b",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_b",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskB.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
                {
                    "name": "task_c",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_c",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskC.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
                {
                    "name": "task_c2",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_c2",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskC2.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
                {
                    # This task is generated  but not included in any variants.
                    "name": "task_d",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_d",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskD.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
            ],
            "buildvariants": [
                {"name": "a", "tasks": [{"name": "task_a"}]},
                {"name": "b", "tasks": [{"name": "task_b"}]},
                {"name": "c", "tasks": [{"name": "task_c"}, {"name": "task_c2"}]},
            ],
            "exec_timeout_secs": 64800,
        }

        workload_files = [
            make_auto_run("TaskA", "a_setup"),
            make_auto_run("TaskB", "b_setup"),
            make_auto_run("TaskC", "c_setup"),
            make_auto_run("TaskC2", "c_setup"),
            make_auto_run("TaskD", "d_setup"),
        ]

        reader: YamlReader = MockReader(workload_files)
        lister: WorkloadLister = MagicMock(name="lister", spec=WorkloadLister, instance=True)

        lister.all_workload_files.return_value = [v.base_name for v in workload_files]
        lister.modified_workload_files.return_value = [
            v.base_name for v in workload_files if v.modified
        ]

        build_infos = [
            CurrentBuildInfo(
                {
                    "build_variant": "a",
                    "mongodb_setup": "a_setup",
                    "execution": "0",
                }
            ),
            CurrentBuildInfo(
                {
                    "build_variant": "b",
                    "mongodb_setup": "b_setup",
                    "execution": "0",
                }
            ),
            CurrentBuildInfo(
                {
                    "build_variant": "c",
                    "mongodb_setup": "c_setup",
                    "execution": "0",
                }
            ),
        ]

        expected = {}

        repo = Repo(lister, reader, workspace_root=".")
        config = create_configuration(repo, build_infos, False, [])

        parsed = json.loads(config.to_json())
        self.assertEqual(EXPECTED, parsed)

    def test_generate_all_activate_tasks(self):
        """
        Tests generating all tasks for a given parsed list of variants in a config file, and
        activating a specific test using activate_generated_tasks.
        """
        self.maxDiff = None
        EXPECTED = {
            "tasks": [
                {
                    "name": "task_a",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_a",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskA.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
                {
                    "name": "task_b",
                    "commands": [
                        TIMEOUT_COMMAND,
                        {
                            "func": "f_run_dsi_workload",
                            "vars": {
                                "test_control": "task_b",
                                "auto_workload_path": "src/genny/src/workloads/src/TaskB.yml",
                            },
                        },
                    ],
                    "priority": 5,
                },
            ],
            "buildvariants": [
                {
                    "name": "a",
                    "tasks": [
                        {
                            "name": "task_a",
                            "activate": True,
                        }
                    ],
                },
                {"name": "b", "tasks": [{"name": "task_b"}]},
            ],
            "exec_timeout_secs": 64800,
        }

        workload_files = [
            make_auto_run("TaskA", "a_setup"),
            make_auto_run("TaskB", "b_setup"),
        ]

        reader: YamlReader = MockReader(workload_files)
        lister: WorkloadLister = MagicMock(name="lister", spec=WorkloadLister, instance=True)

        lister.all_workload_files.return_value = [v.base_name for v in workload_files]
        lister.modified_workload_files.return_value = [
            v.base_name for v in workload_files if v.modified
        ]

        build_infos = [
            CurrentBuildInfo(
                {
                    "build_variant": "a",
                    "mongodb_setup": "a_setup",
                    "execution": "0",
                }
            ),
            CurrentBuildInfo(
                {
                    "build_variant": "b",
                    "mongodb_setup": "b_setup",
                    "execution": "0",
                }
            ),
        ]

        expected = {}

        repo = Repo(lister, reader, workspace_root=".")
        config = create_configuration(repo, build_infos, False, [VariantTask("a", "task_a")])

        parsed = json.loads(config.to_json())
        self.assertEqual(EXPECTED, parsed)

    def test_parse_project_file(self):
        """
        Simple test for parsing a YAML project file from Evergreen. Checks that we can find both
        build variants and get the expansions from each of them.
        """

        YAML_INPUT = """
tasks: 
  - name: bestbuy_agg_merge_target_hashed
    priority: 5
    commands:
      - func: f_run_dsi_workload
        vars:
        test_control: "bestbuy_agg_merge_target_hashed"
buildvariants:
  - name: task_generation
    display_name: Task Generation
    cron: "0 0 1 1 *"  # Every year starting 1/1 at 00:00
    expansions:
      platform: linux
      project_dir: dsi
    run_on:
      - amazon2-build
    tasks:
      - name: schedule_global_auto_tasks
  - name: linux-standalone.2022-11
    display_name: Linux Standalone 2022-11
    cron: "0 0 * * *"  # Everyday at 00:00
    expansions:
      mongodb_setup_release: 2022-11
      mongodb_setup: standalone
      infrastructure_provisioning_release: 2022-11
      infrastructure_provisioning: single
      workload_setup: 2022-11
      platform: linux
      project_dir: dsi
      authentication: enabled
      storageEngine: wiredTiger
      compile_variant: "-arm64"
    run_on:
      - "rhel70-perf-single"
    tasks:
      - name: bestbuy_agg_merge_target_hashed
      - name: schedule_patch_auto_tasks
      - name: schedule_variant_auto_tasks

  - name: linux-standalone-all-feature-flags.2022-11
    display_name: Linux Standalone (all feature flags) 2022-11
    cron: "0 0 * * 2,4,6"  # Tuesday, Thursday and Saturday at 00:00
    expansions:
      mongodb_setup_release: 2022-11
      mongodb_setup: standalone-all-feature-flags
      infrastructure_provisioning_release: 2022-11
      infrastructure_provisioning: single
      workload_setup: 2022-11
      platform: linux
      project_dir: dsi
      authentication: enabled
      storageEngine: wiredTiger
      compile_variant: "-arm64"
    run_on:
      - "rhel70-perf-single"
    tasks:
      - name: schedule_patch_auto_tasks
      - name: schedule_variant_auto_tasks

  - name: do-not-schedule-no-autotasks.2022-11
    display_name: Linux Standalone (all feature flags) 2022-11
    cron: "0 0 * * 2,4,6"  # Tuesday, Thursday and Saturday at 00:00
    expansions:
      mongodb_setup_release: 2022-11
      mongodb_setup: standalone-all-feature-flags
      infrastructure_provisioning_release: 2022-11
      infrastructure_provisioning: single
      workload_setup: 2022-11
      platform: linux
      project_dir: dsi
      authentication: enabled
      storageEngine: wiredTiger
      compile_variant: "-arm64"
    run_on:
      - "rhel70-perf-single"
    tasks:
      - name: some-specific-task
        """

        m = mock_open(read_data=YAML_INPUT)
        reader: YamlReader = YamlReader()

        with patch.object(genny.tasks.auto_tasks_all, "open", m), patch(
            "os.path.exists", lambda x: True
        ):
            all_builds = get_all_builds({"execution": 0, "build_variant": "Generate"}, "ignored")
        self.assertEqual(
            all_builds,
            [
                CurrentBuildInfo(
                    {
                        "execution": 0,
                        "build_variant": "linux-standalone.2022-11",
                        "mongodb_setup_release": "2022-11",
                        "mongodb_setup": "standalone",
                        "infrastructure_provisioning_release": "2022-11",
                        "infrastructure_provisioning": "single",
                        "workload_setup": "2022-11",
                        "platform": "linux",
                        "project_dir": "dsi",
                        "authentication": "enabled",
                        "storageEngine": "wiredTiger",
                        "compile_variant": "-arm64",
                    }
                ),
                CurrentBuildInfo(
                    {
                        "execution": 0,
                        "build_variant": "linux-standalone-all-feature-flags.2022-11",
                        "mongodb_setup_release": "2022-11",
                        "mongodb_setup": "standalone-all-feature-flags",
                        "infrastructure_provisioning_release": "2022-11",
                        "infrastructure_provisioning": "single",
                        "workload_setup": "2022-11",
                        "platform": "linux",
                        "project_dir": "dsi",
                        "authentication": "enabled",
                        "storageEngine": "wiredTiger",
                        "compile_variant": "-arm64",
                    }
                ),
            ],
        )

    def test_parse_activate_generated_tasks(self):
        parsed_tasks = parse_activate_generated_tasks("")
        self.assertEqual(parsed_tasks, set())

        def try_invalid_string():
            parse_activate_generated_tasks("a:b,c")

        self.assertRaisesRegex(
            ValueError, "Invalid value for 'activate_generated_tasks'", try_invalid_string
        )

        parsed_tasks = parse_activate_generated_tasks("variant1:task1,variant2:task2")
        self.assertEqual(
            parsed_tasks, set([VariantTask("variant1", "task1"), VariantTask("variant2", "task2")])
        )


if __name__ == "__main__":
    unittest.main()
