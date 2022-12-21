import json
import shutil
import unittest
import os
from typing import NamedTuple, List, Optional
from unittest.mock import MagicMock
from unittest.mock import patch
import structlog

from genny.tasks.auto_tasks import (
    CLIOperation,
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
        genny_repo_root = os.path.join(self.workspace_root, "/src/genny")
        workload_root = os.path.join(self.workspace_root, "/src/genny")
        # Make them smarter.
        lister.all_workload_files.return_value = [
            v.base_name
            for v in given_files
            # Hack: Prevent calling e.g. expansions.yml a 'workload' file
            # (solution would be to pass in workload files as a different
            # param to this function, but meh)
            if "/" in v.base_name
        ]

        op = CLIOperation(
            genny_repo_root=genny_repo_root,
            workspace_root=self.workspace_root,
            workload_root=workload_root,
        )
        repo = Repo(lister, reader, workspace_root=self.workspace_root)
        tasks = repo.generate_dsi_tasks(op)
        writer = ConfigWriter(op)

        yaml_output = writer.write(tasks, write=False)
        assert then_writes, yaml_output


def expansions_mock(exp_vars) -> MockFile:
    yaml_conts = {"build_variant": "some-build-variant", "execution": "0"}
    yaml_conts.update(exp_vars)
    return MockFile(base_name="expansions.yml", modified=False, yaml_conts=yaml_conts,)


class AutoTasksTests(BaseTestClass):
    def test_all_tasks(self):
        auto_run = {
            "AutoRun": [
                {
                    "When": {"mongodb_setup": {"$eq": "matches"}},
                    "ThenRun": [{"mongodb_setup": "a"}, {"arb_bootstrap_key": "b"}],
                }
            ]
        }
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
        multi_modified_genny = MockFile(
            base_name="src/genny/src/workloads/src/Multi.yml", modified=True, yaml_conts=auto_run,
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

        expected_output = """
  - name: multi_a
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: multi_a
          auto_workload_path: src/genny/src/workloads/src/Multi.yml
          mongodb_setup: a
  -   name: multi_b
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: multi_b
          auto_workload_path: src/genny/src/workloads/src/Multi.yml
          arb_bootstrap_key: b
  -   name: multi_other_a
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: multi_other_a
          auto_workload_path: src/other/src/workloads/src/MultiOther.yml
          mongodb_setup: a
  -   name: multi_other_b
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: multi_other_b
          auto_workload_path: src/other/src/workloads/src/MultiOther.yml
          arb_bootstrap_key: b
  -   name: nested_directory_unmodified_a
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: nested_directory_unmodified_a
          auto_workload_path: src/genny/src/workloads/directory/nested_directory/Unmodified.yml
          mongodb_setup: a
  -   name: nested_directory_unmodified_b
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: nested_directory_unmodified_b
          auto_workload_path: src/genny/src/workloads/directory/nested_directory/Unmodified.yml
          arb_bootstrap_key: b
  -   name: nested_directory_unmodified_other_a
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: nested_directory_unmodified_other_a
          auto_workload_path: src/other/src/workloads/directory/nested_directory/UnmodifiedOther.yml
          mongodb_setup: a
  -   name: nested_directory_unmodified_other_b
      runs_on_variants:
      - matches
      bootstrap_vars:
          test_control: nested_directory_unmodified_other_b
          auto_workload_path: src/other/src/workloads/directory/nested_directory/UnmodifiedOther.yml
          arb_bootstrap_key: b
  """
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
            then_writes=expected_output,
            to_file="./build/DSITasks/DsiTasks-genny.yml",
        )


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
        with patch.object(YamlReader, "load", return_value={"execution": "0"}):
            reader = YamlReader()
            op = CLIOperation(
                genny_repo_root=genny_repo_root,
                workspace_root=workspace_root,
                workload_root=genny_repo_root,
            )
            lister = WorkloadLister(
                workspace_root=workspace_root,
                genny_repo_root=genny_repo_root,
                reader=reader,
                workload_root=genny_repo_root,
            )
            repo = Repo(lister=lister, reader=reader, workspace_root=workspace_root)
            tasks = repo.generate_dsi_tasks(op=op)

            writer = ConfigWriter(op)
            writer.write(tasks, False)
    except Exception as e:
        SLOG.error(
            "'./run-genny auto-tasks' is failing. "
            "This is likely a user error in the auto_tasks syntax. "
            "Refer to error msg for what needs to be fixed."
        )
        raise e


if __name__ == "__main__":
    unittest.main()
