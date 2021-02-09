import structlog
import os
import shutil

from typing import Callable, TypeVar, Tuple, Optional

from genny import curator, cmd_runner, toolchain

SLOG = structlog.get_logger(__name__)

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


def cmake_test(genny_repo_root: str, workspace_root: str):
    info = toolchain.toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    workdir = os.path.join(genny_repo_root, "build")

    ctest_cmd = [
        "ctest",
        "--verbose",
        "--label-exclude",
        "(standalone|sharded|single_node_replset|three_node_replset|benchmark)",
    ]

    def cmd_func() -> bool:
        output: cmd_runner.RunCommandOutput = cmd_runner.run_command(
            cmd=ctest_cmd, cwd=workdir, env=info.toolchain_env, capture=False, check=True
        )
        return output.returncode == 0

    _run_command_with_sentinel_report(
        cmd_func=cmd_func, workspace_root=workspace_root, genny_repo_root=genny_repo_root
    )


def benchmark_test(genny_repo_root: str, workspace_root: str):
    info = toolchain.toolchain_info(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    workdir = os.path.join(genny_repo_root, "build")

    ctest_cmd = ["ctest", "--label-regex", "(benchmark)"]

    def cmd_func():
        cmd_runner.run_command(
            cmd=ctest_cmd, cwd=workdir, env=info.toolchain_env, capture=False, check=True
        )

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

        expected_to_find = {"100 == 101", "1 failed"}

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


# See the logic in _setup_resmoke.
# These are the "Binaries" evergreen artifact URLs for mongodb-mongo compile tasks.
# The binaries must be compatible with the version of the mongo repo checked out in use for resmoke,
# which is the sha "298d4d6bbb9980b74bded06241067fe6771bef68" mentioned below.
_canned_artifacts = {
    "osx": "https://mciuploads.s3.amazonaws.com/mongodb-mongo-master/macos/298d4d6bbb9980b74bded06241067fe6771bef68/binaries/mongo-mongodb_mongo_master_macos_298d4d6bbb9980b74bded06241067fe6771bef68_20_10_22_00_55_19.tgz",
    "amazon2": "https://mciuploads.s3.amazonaws.com/mongodb-mongo-master/amazon2/298d4d6bbb9980b74bded06241067fe6771bef68/binaries/mongo-mongodb_mongo_master_amazon2_298d4d6bbb9980b74bded06241067fe6771bef68_20_10_22_00_55_19.tgz",
}


def _setup_resmoke(
    workspace_root: str,
    genny_repo_root: str,
    mongo_dir: Optional[str],
    mongodb_archive_url: Optional[str],
):
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
    if not os.path.exists(mongo_repo_path):
        SLOG.info("Mongo repo doesn't exist. Checking it out.", mongo_repo_path=mongo_repo_path)
        cmd_runner.run_command(
            cmd=["git", "clone", "git@github.com:mongodb/mongo.git", mongo_repo_path],
            cwd=workspace_root,
            check=True,
            capture=False,
        )
        cmd_runner.run_command(
            # If changing this sha, you may need to use later binaries
            # in the _canned_artifacts dict.
            cmd=["git", "checkout", "298d4d6bbb9980b74bded06241067fe6771bef68"],
            cwd=mongo_repo_path,
            check=True,
            capture=False,
        )
    else:
        SLOG.info("Using existing mongo repo checkout", mongo_repo_path=mongo_repo_path)
        cmd_runner.run_command(
            cmd=["git", "rev-parse", "HEAD"], check=False, cwd=mongo_repo_path, capture=False,
        )

    # Look for mongod in
    # build/opt/mongo/db/mongod
    # build/install/bin/mongod
    # bin/
    opt = os.path.join(mongo_repo_path, "build", "opt", "mongo", "db", "mongod")
    install = os.path.join(mongo_repo_path, "build", "install", "bin", "mongod")
    from_tarball = os.path.join(mongo_repo_path, "bin", "mongod")
    if os.path.exists(opt):
        mongod = opt
    elif os.path.exists(install):
        mongod = install
    elif os.path.exists(from_tarball):
        mongod = from_tarball
    else:
        mongod = None

    if mongod is not None and mongodb_archive_url is not None:
        SLOG.info(
            "Found exisitng mongod so will not download artifacts.",
            existing_mongod=mongod,
            wont_download_artifacts_from=mongodb_archive_url,
        )

    if mongod is None:
        SLOG.info(
            "Couldn't find pre-build monogod. Fetching and installing.",
            looked_at=(opt, install, from_tarball),
            fetching=mongodb_archive_url,
        )
        if mongodb_archive_url is None:
            info = toolchain.toolchain_info(
                genny_repo_root=genny_repo_root, workspace_root=workspace_root
            )

            if info.is_darwin:
                artifact_key = "osx"
            elif info.linux_distro == "amazon2":
                artifact_key = "amazon2"
            else:
                raise Exception(
                    f"No pre-built artifacts for distro {info.linux_distro}. You can either:"
                    f"1. compile/install a local mongo checkout in ./src/mongo."
                    f"2. Modify the _canned_artifacts dict in the genny python to include an artifact from a waterfall build."
                    f"3. Pass in the --mongodb-archive-url parameter to force a canned artifact."
                )
            mongodb_archive_url = _canned_artifacts[artifact_key]

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

    # Setup resmoke venv unless exists
    resmoke_setup_sentinel = os.path.join(resmoke_venv, "setup-done")
    if not os.path.exists(resmoke_setup_sentinel):
        SLOG.info("Resmoke venv doesn't exist. Creating.", resmoke_venv=resmoke_venv)
        shutil.rmtree(resmoke_venv, ignore_errors=True)
        import venv

        venv.create(env_dir=resmoke_venv, with_pip=True, symlinks=True)
        reqs_file = os.path.join(mongo_repo_path, "etc", "pip", "evgtest-requirements.txt")

        cmd = [resmoke_python, "-mpip", "install", "-r", reqs_file]
        cmd_runner.run_command(
            cmd=cmd, cwd=workspace_root, capture=False, check=True,
        )

        open(resmoke_setup_sentinel, "w")

    return resmoke_python, mongo_repo_path, bin_dir


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
        checker_func = None

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
        os.path.join(mongo_repo_path, "buildscripts", "resmoke.py"),
        "run",
        "--suite",
        suites,
        "--configDir",
        os.path.join(mongo_repo_path, "buildscripts", "resmokeconfig"),
        "--mongod",
        mongod,
        "--mongo",
        mongo,
        "--mongos",
        mongos,
    ]

    def run_resmoke() -> None:
        # See if we can put this in the resmoke suite def or something?
        env["CTEST_OUTPUT_ON_FAILURE"] = 1
        cmd_runner.run_command(
            cmd=cmd,
            cwd=workspace_root,
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
