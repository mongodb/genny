import platform
import sys
import structlog
import os
import shutil

import toolchain
from cmd_runner import run_command

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


def _run_command_with_sentinel_report(cmd_func, checker_func=None):
    # This can only be imported after the setup script has installed gennylib.
    from curator import poplar_grpc

    # TODO: use path relative to genny_repo_root
    sentinel_file = os.path.join(os.getcwd(), "build", "sentinel.junit.xml")

    with open(sentinel_file, "w") as f:
        f.write(_sentinel_report)

    with poplar_grpc(cleanup_metrics=True):
        res = cmd_func()

    if checker_func:
        success = checker_func()
    else:
        success = res.returncode == 0

    if success:
        SLOG.debug("Test succeeded, removing sentinel report")
        os.remove(sentinel_file)
    else:
        SLOG.debug("Test failed, leaving sentinel report in place")


def cmake_test(
    genny_repo_root: str, env: dict,
):
    workdir = os.path.join(genny_repo_root, "build")

    ctest_cmd = [
        "ctest",
        "--verbose",
        "--label-exclude",
        "(standalone|sharded|single_node_replset|three_node_replset|benchmark)",
    ]

    _run_command_with_sentinel_report(
        lambda: run_command(cmd=ctest_cmd, cwd=workdir, env=env, capture=False)
    )


def benchmark_test(
    genny_repo_root: str, env: dict,
):
    workdir = os.path.join(genny_repo_root, "build")

    ctest_cmd = ["ctest", "--label-regex", "(benchmark)"]

    _run_command_with_sentinel_report(
        lambda: run_command(cmd=ctest_cmd, cwd=workdir, env=env, capture=False)
    )


def _check_create_new_actor_test_report(workdir):
    passed = False

    report_file = os.path.join(workdir, "build", "create_new_actor_test.junit.xml")

    if not os.path.isfile(report_file):
        SLOG.error("Failed to find report file", report_file=report_file)
        return passed

    expected_error = 'failure message="100 == 101"'

    with open(report_file) as f:
        report = f.read()
        passed = expected_error in report

    if passed:
        os.remove(report_file)  # Remove the report file for the expected failure.
    else:
        SLOG.error(
            "test for create-new-actor script did not succeed. Failed to find expected "
            "error message in report file",
            expected_to_find=expected_error,
        )

    return passed


def resmoke_test(
    genny_repo_root: str, suites: str, is_cnats: bool, mongo_dir: str, env: dict,
):
    if (not suites) and (not is_cnats):
        raise ValueError('Must specify either "--suites" or "--create-new-actor-test-suite"')

    if is_cnats:
        suites = os.path.join(genny_repo_root, "src", "resmokeconfig", "genny_create_new_actor.yml")
        checker_func = lambda: _check_create_new_actor_test_report(genny_repo_root)
    else:
        checker_func = None

    if mongo_dir is not None:
        mongo_repo_path = mongo_dir
    else:
        evergreen_mongo_repo = os.path.join(genny_repo_root, "..", "mongo")
        run_from_evergreen_workspace = os.getcwd() != genny_repo_root

        if os.path.exists(evergreen_mongo_repo) and run_from_evergreen_workspace:
            mongo_repo_path = evergreen_mongo_repo
        else:
            mongo_repo_path = os.path.join(genny_repo_root, "build", "resmoke-mongo")

    resmoke_venv: str = os.path.join(mongo_repo_path, "resmoke_venv")
    resmoke_python: str = os.path.join(resmoke_venv, "bin", "python3")

    # Clone repo unless exists
    if not os.path.exists(mongo_repo_path):
        run_command(
            cmd=["git", "clone", "git@github.com:mongodb/mongo.git", mongo_repo_path], capture=False
        )

        # See comment in evergreen.yml - mongodb_archive_url
        run_command(
            cmd=["git", "checkout", "298d4d6bbb9980b74bded06241067fe6771bef68"],
            cwd=mongo_repo_path,
            capture=False,
        )

    # Setup resmoke venv unless exists
    resmoke_setup_sentinel = os.path.join(resmoke_venv, "setup-done")
    if not os.path.exists(resmoke_setup_sentinel):
        shutil.rmtree(resmoke_venv, ignore_errors=True)
        import venv

        venv.create(env_dir=resmoke_venv, with_pip=True, symlinks=True)
        reqs_file = os.path.join(mongo_repo_path, "etc", "pip", "evgtest-requirements.txt")
        cmd = [resmoke_python, "-mpip", "install", "-r", reqs_file]
        run_command(cmd=cmd, capture=False)
        open(resmoke_setup_sentinel, "w")

    cmd = [
        resmoke_python,
        os.path.join(mongo_repo_path, "buildscripts", "resmoke.py"),
        "run",
        "--suite",
        suites,
        "--configDir",
        os.path.join(mongo_repo_path, "buildscripts", "resmokeconfig"),
        "--mongod",
        "mongod",
        "--mongo",
        "mongo",
        "--mongos",
        "mongos",
    ]

    _run_command_with_sentinel_report(
        lambda: run_command(cmd=cmd, cwd=genny_repo_root, env=env), checker_func, capture=False
    )


def _check_venv():
    if "VIRTUAL_ENV" not in os.environ:
        SLOG.error("Tried to execute without active virtualenv.")
        sys.exit(1)


def run_self_test():
    _check_venv()
    _validate_environment()

    import pytest

    pytest.main([])


def _python_version_string():
    return ".".join(map(str, sys.version_info))[0:5]


def _validate_environment():
    # Check Python version
    if not sys.version_info >= (3, 7):
        raise OSError(
            "Detected Python version {version} less than 3.7. Please delete "
            "the virtualenv and run lamp again.".format(version=_python_version_string())
        )

    # Check the macOS version. Non-mac platforms return a tuple of empty strings
    # for platform.mac_ver().
    if platform.mac_ver()[0] == "10":
        release_triplet = platform.mac_ver()[0].split(".")
        if int(release_triplet[1]) < 14:
            # You could technically compile clang or gcc yourself on an older version
            # of macOS, but it's untested so we might as well just enforce
            # a blanket minimum macOS version for simplicity.
            SLOG.error("Genny requires macOS 10.14 Mojave or newer")
            sys.exit(1)
    return
