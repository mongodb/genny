import os
import subprocess


def cmake_test(env):
    # We rely on catch2 to report test failures, but it doesn't always do so.
    # See https://github.com/catchorg/Catch2/issues/1210
    # As a workaround, we generate a dummy report with a failed test that is
    # deleted if the test succeeds.

    sentinel_report = """
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

    workdir = os.path.join(os.getcwd(), 'build')
    sentinel_file = os.path.join(workdir, 'sentinel.junit.xml')

    with open(sentinel_file, 'w') as f:
        f.write(sentinel_report)

    ctest_cmd = [
        'ctest',
        '--label-exclude',
        '(standalone|sharded|single_node_replset|three_node_replset)'
    ]
    res = subprocess.run(ctest_cmd, cwd=workdir, env=env)

    if res.returncode == 0:
        os.remove(sentinel_file)
