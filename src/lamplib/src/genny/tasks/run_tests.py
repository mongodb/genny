import os
import re
import shutil
import subprocess
import sys
from typing import Callable, Optional, Tuple, TypeVar

import structlog

from genny import cmd_runner, curator, toolchain

SLOG = structlog.get_logger(__name__)

# See the logic in _setup_resmoke.
# These are the "Binaries" evergreen artifact URLs for mongodb-mongo compile tasks.
# The binaries must be compatible with the version of the mongo repo checked out in use for resmoke,
# which is the sha "6e2dcc5a39eb2de9d2e8209271115c078ae16470" mentioned below.
MONGO_COMMIT = "e61bf27c2f6a83fed36e5a13c008a32d563babe2"
CANNED_ARTIFACTS = {
    "osx": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-macos-x86_64-6.0.0.tgz",
    "amazon2": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-linux-x86_64-amazon2-6.0.0.tgz",
    "amazon2_arm64": "https://fastdl.mongodb.org/linux/mongodb-linux-aarch64-amazon2-6.0.0.tgz",
    "amazon2023": "https://fastdl.mongodb.org/linux/mongodb-linux-x86_64-amazon2023-7.0.2.tgz",
    "amazon2023_arm64": "https://fastdl.mongodb.org/linux/mongodb-linux-aarch64-amazon2023-7.0.2.tgz",
    "ubuntu1804": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-linux-x86_64-ubuntu1804-6.0.0.tgz",
    "ubuntu2004": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-linux-x86_64-ubuntu2004-6.0.0.tgz",
    "rhel70": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-linux-x86_64-rhel70-6.0.0.tgz",
    "rhel8": "https://dsi-donot-remove.s3.us-west-2.amazonaws.com/compile_artifacts/mongodb-linux-x86_64-rhel80-6.0.0.tgz",
    "ubuntu2204_arm64": "https://fastdl.mongodb.org/linux/mongodb-linux-aarch64-ubuntu2204-6.0.4.tgz",
    "ubuntu2204": "https://fastdl.mongodb.org/linux/mongodb-linux-x86_64-ubuntu2204-6.0.4.tgz",
}

# We rely on catch2 to report test failures, but it doesn't always do so.
# See https://github.com/catchorg/Catch2/issues/1210
# As a workaround, we generate a dummy report with a failed test that is
# deleted if the test succeeds.
_sentinel_report = """
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
 <testsuite name="test_failure_sentinel" errors="0" failures="1" tests="1" hostname="tbd" time="1.0" timestamp="2019-01-01T00:00:00Z">
  <testcase classname="test_failure_sentinel" name="A test failed early and a report was not generated" time="1.0">
   <failure message="test did not exit cleanly, see task log for detail" type="">
   </failure>
  </testcase>
  <system-out/>
  <system-err/>
 </testsuite>
</testsuites>
""".strip()


T = TypeVar("T")


def _outcome_was_true(outcome: bool) -> bool:
    return outcome


def _run_command_with_sentinel_report(
    genny_repo_root: str,
    workspace_root: str,
    cmd_func: Callable[..., T],
    checker_func: Callable[[T], bool] = None,
) -> Tuple[T, bool]:
    if checker_func is None:
        checker_func = _outcome_was_true

    sentinel_file = os.path.join(workspace_root, "build", "XUnitXML", "sentinel.junit.xml")
    os.makedirs(name=os.path.dirname(sentinel_file), exist_ok=True)

    success = False
    try:
        with open(sentinel_file, "w") as f:
            f.write(_sentinel_report)
            SLOG.debug("Created sentinel file", sentinel_file=sentinel_file)

        with curator.poplar_grpc(
            cleanup_metrics=True, workspace_root=workspace_root, genny_repo_root=genny_repo_root
        ):
            cmd_output = cmd_func()

        success = checker_func(cmd_output)
        return cmd_output, success
    finally:
        if success and os.path.exists(sentinel_file):
            os.remove(sentinel_file)
        SLOG.debug(
            "Command finished. Left sentinel_file in place unless success.",
            success=success,
            sentinel_file=sentinel_file,
        )


def cmake_test(
    genny_repo_root: str,
    workspace_root: str,
    regex=None,
    regex_exclude=None,
    repeat_until_fail: int = 1,
):
    info = toolchain.toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    workdir = os.path.join(genny_repo_root, "build")

    xunit_dir = os.path.join(workspace_root, "build", "XUnitXML")
    os.makedirs(xunit_dir, exist_ok=True)

    ctest_cmd = [
        "ctest",
        "--schedule-random",
        "--output-on-failure",
        "--parallel",
        "4",
        "--repeat-until-fail",
        str(repeat_until_fail),
        "--label-exclude",
        "(standalone|sharded|single_node_replset|three_node_replset|benchmark)",
    ]

    if regex is not None:
        ctest_cmd += ["--tests-regex", regex]

    if regex_exclude is not None:
        ctest_cmd += ["--exclude-regex", regex_exclude]

    def cmd_func() -> bool:
        output: cmd_runner.RunCommandOutput = cmd_runner.run_command(
            cmd=ctest_cmd, cwd=workdir, env=info.toolchain_env, capture=False, check=True
        )
        return output.returncode == 0

    try:
        _run_command_with_sentinel_report(
            cmd_func=cmd_func, workspace_root=workspace_root, genny_repo_root=genny_repo_root
        )

    except subprocess.CalledProcessError:
        sys.exit(1)


def benchmark_test(genny_repo_root: str, workspace_root: str):
    info = toolchain.toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    workdir = os.path.join(genny_repo_root, "build")

    ctest_cmd = ["ctest", "--label-regex", "(benchmark)"]

    def cmd_func():
        output: cmd_runner.RunCommandOutput = cmd_runner.run_command(
            cmd=ctest_cmd, cwd=workdir, env=info.toolchain_env, capture=False, check=True
        )
        return output.returncode == 0

    _run_command_with_sentinel_report(
        cmd_func=cmd_func, workspace_root=workspace_root, genny_repo_root=genny_repo_root
    )


def _check_create_new_actor_test_report(workspace_root: str) -> Callable[[str], bool]:
    def out(cmd_output: str) -> bool:
        passed = False

        report_file = os.path.join(
            workspace_root, "build", "XUnitXML", "create_new_actor_test.junit.xml"
        )

        if not os.path.isfile(report_file):
            SLOG.error("Failed to find report file", report_file=report_file)
            return passed

        expected_to_find = {"100 == 101", 'failures="1"'}

        with open(report_file) as f:
            report = f.read()
            passed = all(ex in report for ex in expected_to_find)

        if passed:
            SLOG.debug("Test passed. Removing report file.", report_file=report_file)
            os.remove(report_file)  # Remove the report file for the expected failure.
        else:
            SLOG.error(
                "test for create-new-actor script did not succeed. Failed to find expected "
                "messages in report file",
                expected_to_find=expected_to_find,
            )

        return passed

    return out


def _get_mongo_commit(mongod_path, genny_repo_root):
    mongo_version_cmd = [mongod_path, "--version"]
    workdir = os.path.join(genny_repo_root, "build")
    output: cmd_runner.RunCommandOutput = cmd_runner.run_command(
        cmd=mongo_version_cmd, cwd=workdir, capture=True, check=True
    )
    commit_regex = r"\"gitVersion\":\s\"([\da-z]+)\""
    commit = None
    if output.returncode == 0:
        matches = re.search(commit_regex, str(output.stdout), re.MULTILINE)
        if matches:
            commit = matches.group(1)
    SLOG.info(f"Identified Mongo commit", commit=commit)
    return commit


def _setup_resmoke(
    workspace_root: str,
    genny_repo_root: str,
    mongo_dir: Optional[str],
    mongodb_archive_url: Optional[str],
):
    # If artifact is not overridden by user then use the default commit
    expected_commit = None
    if mongodb_archive_url is None:
        expected_commit = MONGO_COMMIT

    if mongo_dir is not None:
        mongo_repo_path = mongo_dir
    else:
        evergreen_mongo_repo = os.path.join(workspace_root, "src", "mongo")
        if os.path.exists(evergreen_mongo_repo):
            mongo_repo_path = evergreen_mongo_repo
        else:
            mongo_repo_path = os.path.join(genny_repo_root, "build", "resmoke-mongo")

    xunit_xml_path = os.path.join(workspace_root, "build", "XUnitXML")
    os.makedirs(xunit_xml_path, exist_ok=True)
    SLOG.info("Created xunit result dir", path=xunit_xml_path)

    resmoke_venv: str = os.path.join(mongo_repo_path, "resmoke_venv")
    resmoke_python: str = os.path.join(resmoke_venv, "bin", "python3")

    # Clone repo unless exists
    checkout_required = False
    if not os.path.exists(mongo_repo_path):
        SLOG.info("Mongo repo doesn't exist. Checking it out.", mongo_repo_path=mongo_repo_path)
        cmd_runner.run_command(
            cmd=["git", "clone", "https://github.com/mongodb/mongo.git", mongo_repo_path],
            cwd=workspace_root,
            check=True,
            capture=False,
        )
        checkout_required = True

    # Download and install mongod if it already doesn't exist or is not the expected version
    from_tarball = os.path.join(mongo_repo_path, "bin", "mongod")
    download_required = True
    if os.path.exists(from_tarball):
        mongodb_commit = _get_mongo_commit(from_tarball, workspace_root)
        if mongodb_commit == expected_commit:
            SLOG.info("Mongo binary exist and is the correct version")
            download_required = False
        else:
            SLOG.info(
                "Mongo binary exist, but is not the correct version. Mongo will be downloaded from the canned artifact."
            )

    if download_required:
        info = toolchain.toolchain_info(
            genny_repo_root=genny_repo_root, workspace_root=workspace_root
        )
        if mongodb_archive_url is None:
            if info.is_darwin:
                artifact_key = "osx"
            elif info.linux_distro in CANNED_ARTIFACTS:
                artifact_key = info.linux_distro
            else:
                raise Exception(
                    f"No pre-built artifacts for distro {info.linux_distro}. You can either:"
                    f"1. Modify the CANNED_ARTIFACTS dict in the genny python to include an artifact from a waterfall build."
                    f"2. Pass in the --mongodb-archive-url parameter to force a canned artifact."
                )
            mongodb_archive_url = CANNED_ARTIFACTS[artifact_key]

        SLOG.info("Fetching and installing mongod.", fetching=mongodb_archive_url)
        cmd_runner.run_command(
            cmd=["curl", "-LSs", mongodb_archive_url, "-o", "mongodb.tgz"],
            cwd=mongo_repo_path,
            capture=False,
            check=True,
        )
        cmd_runner.run_command(
            cmd=["tar", "--strip-components=1", "-zxf", "mongodb.tgz"],
            cwd=mongo_repo_path,
            capture=False,
            check=True,
        )

    mongod = from_tarball
    bin_dir = os.path.dirname(mongod)

    if checkout_required:
        # checking out the commit corresponding to the mongod binary
        # only required if the local repo didn't exist and had to be cloned
        mongodb_commit = _get_mongo_commit(mongod, workspace_root)
        cmd_runner.run_command(
            cmd=["git", "checkout", mongodb_commit],
            cwd=mongo_repo_path,
            check=True,
            capture=False,
        )

    # Setup resmoke venv unless exists
    resmoke_setup_sentinel = os.path.join(resmoke_venv, "setup-done")
    if not os.path.exists(resmoke_setup_sentinel):
        SLOG.info("Resmoke venv doesn't exist. Creating.", resmoke_venv=resmoke_venv)
        shutil.rmtree(resmoke_venv, ignore_errors=True)
        import venv

        venv.create(env_dir=resmoke_venv, with_pip=True, symlinks=True)
        reqs_file = os.path.join(mongo_repo_path, "etc", "pip", "evgtest-requirements.txt")

        # Cython >=3 is not compatable with PyYaml <=6.0.1 && >5.3.1. Because we
        # don't control this requirements.txt, we'll pre-install a pyyaml AND a
        # Cython that works, and hope that one of them survive. We do the work
        # here to make sure that old MongoDB versions are compatible with New
        # Genny.
        #
        # If this requirements file is ever upgraded to account for this, the
        # dependencies manually installed in this line should automatically
        # upgraded. This line is to make sure that a working set of
        # dependencies is _already_ installed so that pip wont bother upgrading
        # them.
        #
        # See: https://jira.mongodb.org/browse/EVG-20471
        # See: https://github.com/yaml/pyyaml/issues/601
        cmd = [
            resmoke_python,
            "-mpip",
            "install",
            "Cython<3.0",
            "PyYAML<=5.3.1",
            "--no-build-isolation",
        ]
        cmd_runner.run_command(
            cmd=cmd,
            cwd=workspace_root,
            capture=False,
            check=True,
        )

        # Now install the dependencies in the requirements file like normal
        cmd = [resmoke_python, "-mpip", "install", "-r", reqs_file]
        cmd_runner.run_command(
            cmd=cmd,
            cwd=workspace_root,
            capture=False,
            check=True,
        )

        open(resmoke_setup_sentinel, "w")

    return resmoke_python, mongo_repo_path, bin_dir


def _nop_true(cmd_output: str) -> bool:
    return True


def resmoke_test(
    genny_repo_root: str,
    workspace_root: str,
    suites: str,
    is_cnats: bool,
    mongo_dir: Optional[str],
    env: dict,
    mongodb_archive_url: Optional[str],
):
    if (not suites) and (not is_cnats):
        raise ValueError('Must specify either "--suites" or "--create-new-actor-test-suite"')

    if is_cnats:
        suites = os.path.join(genny_repo_root, "src", "resmokeconfig", "genny_create_new_actor.yml")
        checker_func = _check_create_new_actor_test_report(workspace_root=workspace_root)
    else:
        if genny_repo_root not in suites:
            suites = os.path.join(genny_repo_root, suites)
        checker_func = _nop_true

    resmoke_python, mongo_repo_path, bin_dir = _setup_resmoke(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        mongo_dir=mongo_dir,
        mongodb_archive_url=mongodb_archive_url,
    )

    mongod = os.path.join(bin_dir, "mongod")
    mongo = os.path.join(bin_dir, "mongo")
    mongos = os.path.join(bin_dir, "mongos")

    cmd = [
        resmoke_python,
        os.path.join("buildscripts", "resmoke.py"),
        "--configDir",
        os.path.join("buildscripts", "resmokeconfig"),
        "run",
        "--suite",
        suites,
        "--mongod",
        mongod,
        "--mongo",
        mongo,
        "--mongos",
        mongos,
    ]

    def run_resmoke() -> None:
        # See if we can put this in the resmoke suite def or something?
        env["CTEST_OUTPUT_ON_FAILURE"] = "1"
        cmd_runner.run_command(
            cmd=cmd,
            cwd=mongo_repo_path,
            env=env,
            capture=False,
            # If we're create_new_actor_test we don't want
            # to barf when resmoke fails. We expect it to fail.
            check=False if is_cnats else True,  # `not is_cnats` was hard to read.
        )

    _run_command_with_sentinel_report(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        cmd_func=run_resmoke,
        checker_func=checker_func,
    )
