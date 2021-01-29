import subprocess
import structlog
from typing import List, NamedTuple
import os
import shlex
from uuid import uuid4

SLOG = structlog.get_logger(__name__)


class RunCommandOutput(NamedTuple):
    returncode: int
    stdout: List[str]
    stderr: List[str]


def run_command(
    cmd: List[str],
    check: bool,
    cwd: str,
    shell: bool = False,
    env: dict = None,
    capture: bool = True,
) -> RunCommandOutput:
    env = os.environ.copy() if env is None else env

    uuid = str(uuid4())[:8]
    SLOG.debug("Running command", uuid=uuid, cwd=cwd, command=" ".join(shlex.quote(x) for x in cmd))

    success = False

    old_cwd = os.getcwd()
    try:
        if not os.path.exists(cwd):
            raise Exception(f"Cannot chdir to {cwd} from cwd={os.getcwd()}")
        os.chdir(cwd)
        result: subprocess.CompletedProcess = subprocess.run(
            cmd,
            env=env,
            shell=shell,
            check=check,
            text=capture,  # capture implies text. No binary output from genny.
            capture_output=capture,
        )
        success = result.returncode == 0
        return RunCommandOutput(
            returncode=result.returncode,
            stdout=[] if not capture else result.stdout.strip().split("\n"),
            stderr=[] if not capture else result.stderr.strip().split("\n"),
        )

    except subprocess.CalledProcessError as e:
        SLOG.error(
            "Error in command",
            uuid=uuid,
            cmd=cmd,
            env=env,
            cwd=cwd,
            returncode=e.returncode,
            output=e.output,
        )
        raise e
    finally:
        SLOG.debug("Finished command", uuid=uuid, success=success)
        os.chdir(old_cwd)
