import subprocess
import structlog
from typing import List
import os
from uuid import uuid4

SLOG = structlog.get_logger(__name__)


def _short_path(path):
    if path is None:
        return None
    bits = path.split(":")
    if len(bits) <= 3:
        return path
    return ":".join(bits[:4]) + "..."


def run_command(
    cmd: List[str], cwd: str = None, shell: bool = False, env: dict = None, capture: bool = True,
) -> List[str]:
    cwd = os.getcwd() if cwd is None else cwd
    env = os.environ.copy() if env is None else env

    kwargs = dict(env=env, shell=shell, text=capture, check=True, capture_output=capture)

    uuid = str(uuid4())[:8]
    SLOG.debug(
        "Running command", uuid=uuid, cmd=cmd, PATH=_short_path(env.get("PATH", None)), cwd=cwd
    )

    success = False

    old_cwd = os.getcwd()
    try:
        if not os.path.exists(cwd):
            raise Exception(f"Cannot chdir to {cwd} from cwd={os.getcwd()}")
        os.chdir(cwd)
        result: subprocess.CompletedProcess = subprocess.run(cmd, **kwargs)
        success = True
        if not capture or result.stdout == "":
            return []
        return result.stdout.strip().split("\n")
    except subprocess.CalledProcessError as e:
        SLOG.error(
            "Error in command",
            uuid=uuid,
            cmd=cmd,
            env=env,
            cwd=os.getcwd(),
            returncode=e.returncode,
            output=e.output,
        )
        raise e
    finally:
        SLOG.debug("Finished command", uuid=uuid, success=success)
        os.chdir(old_cwd)
