import structlog

import os
import subprocess

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

    sentinel_file = os.path.join(os.getcwd(), "build", "sentinel.junit.xml")

    with open(sentinel_file, "w") as f:
        f.write(_sentinel_report)

    with poplar_grpc():
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


def cmake_test(env):
    workdir = os.path.join(os.getcwd(), "build")

    ctest_cmd = [
        "ctest",
        "--verbose",
        "--label-exclude",
        "(standalone|sharded|single_node_replset|three_node_replset|benchmark)",
    ]

    _run_command_with_sentinel_report(lambda: subprocess.run(ctest_cmd, cwd=workdir, env=env))


def benchmark_test(env):
    workdir = os.path.join(os.getcwd(), "build")

    ctest_cmd = ["ctest", "--label-regex", "(benchmark)"]

    _run_command_with_sentinel_report(lambda: subprocess.run(ctest_cmd, cwd=workdir, env=env))


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


def resmoke_test(env, suites, mongo_dir, is_cnats):
    workdir = os.getcwd()
    checker_func = None

    if is_cnats:
        suites = os.path.join(workdir, "src", "resmokeconfig", "genny_create_new_actor.yml")
        checker_func = lambda: _check_create_new_actor_test_report(workdir)

    if (not suites) and (not is_cnats):
        raise ValueError('Must specify either "--suites" or "--create-new-actor-test-suite"')

    if not mongo_dir:
        # Default mongo directory in Evergreen.
        mongo_dir = os.path.join(workdir, "build", "mongo")
        # Default download location for MongoDB binaries.
        env["PATH"] = os.path.join(mongo_dir, "bin") + ":" + mongo_dir + ":" + env["PATH"]

    if "SCRIPTS_DIR" not in os.environ:
        raise ValueError(
            "The venv directory is required for resmoke and does not exist, "
            "please ensure you're not running Genny with the --run-global flag"
        )

    evg_venv_dir = os.path.join(os.environ["SCRIPTS_DIR"], "venv")

    cmds = []
    if os.path.isdir(evg_venv_dir):
        cmds.append("source " + os.path.join(evg_venv_dir, "bin", "activate"))

    cmds.append(
        " ".join(
            [
                "python",
                os.path.join(mongo_dir, "buildscripts", "resmoke.py"),
                "run",
                "--suite",
                suites,
                "--configDir",
                os.path.join(mongo_dir, "buildscripts", "resmokeconfig"),
                "--mongod mongod --mongo mongo --mongos mongos",
            ]
        )
    )

    _run_command_with_sentinel_report(
        lambda: subprocess.run(
            ";".join(cmds), cwd=workdir, env=env, shell=True, executable="/bin/bash"
        ),
        checker_func,
    )
