import sys
import errno
import socket
import os
import shutil
import subprocess
import datetime
import time
from uuid import uuid4
from typing import Optional, TextIO

import structlog
from contextlib import contextmanager

from genny.cmd_runner import run_command, RunCommandOutput
from genny.download import Downloader

SLOG = structlog.get_logger(__name__)


def _find_curator(workspace_root: str, genny_repo_root: str) -> Optional[str]:
    in_bin = os.path.join(workspace_root, "bin", "curator")
    if os.path.exists(in_bin):
        return in_bin

    in_build = os.path.join(genny_repo_root, "build", "curator", "curator")
    if os.path.exists(in_build):
        return in_build

    return None


def _get_poplar_args(genny_repo_root: str, workspace_root: str):
    """
    Returns the argument list used to create the Poplar gRPC process.

    If we are in the root of the genny repo, use the local executable.
    Otherwise, we search the PATH.
    """
    curator = _find_curator(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    if curator is None:
        raise Exception(
            f"Curator not found in genny_repo_root={genny_repo_root}, "
            f"workspace_root={workspace_root}. "
            f"Ensure you have run install."
        )
    return [curator, "poplar", "grpc"]


def _get_export_args(
    genny_repo_root: str, workspace_root: str, input_path: str, output_path: Optional[str] = None
):
    """
    Returns the argument list used to export ftdc files to csv.

    If we are in the root of the genny repo, use the local executable.
    Otherwise we search the PATH.
    """
    curator = _find_curator(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    if curator is None:
        raise Exception(
            f"Curator not found in genny_repo_root={genny_repo_root}, "
            f"workspace_root={workspace_root}. "
            f"Ensure you have run install."
        )
    output_args = ["--output", output_path] if output_path is not None else []
    return [curator, "ftdc", "export", "csv", "--input", input_path] + output_args


def _get_translate_args(
    genny_repo_root: str, workspace_root: str, input_path: str, output_path: Optional[str] = None
):
    """
    Returns the argument list used to export genny workload files to ftdc.

    If we are in the root of the genny repo, use the local executable.
    Otherwise we search the PATH.
    """
    curator = _find_curator(genny_repo_root=genny_repo_root, workspace_root=workspace_root)
    if curator is None:
        raise Exception(
            f"Curator not found in genny_repo_root={genny_repo_root}, "
            f"workspace_root={workspace_root}. "
            f"Ensure you have run install."
        )
    output_args = ["--output", output_path] if output_path is not None else []
    return [curator, "ftdc", "export", "t2", "--input", input_path] + output_args


_DATE_FORMAT = "%Y-%m-%dT%H%M%SZ"
_METRICS_PATH = "build/WorkloadOutput/CedarMetrics"


def _cleanup_metrics():
    if not os.path.exists(_METRICS_PATH):
        return
    uuid = str(uuid4())[:8]
    ts = datetime.datetime.utcnow().strftime(_DATE_FORMAT)
    dest = f"{_METRICS_PATH}-{ts}-{uuid}"
    shutil.move(_METRICS_PATH, dest)
    SLOG.info(
        "Moved existing metrics (presumably from a prior run).",
        existing=_METRICS_PATH,
        moved_to=dest,
        cwd=os.getcwd(),
    )


def _create_metrics():
    os.makedirs(_METRICS_PATH, exist_ok=True)


def export(
    workspace_root: str, genny_repo_root: str, input_path: str, output_path: Optional[str] = None
):
    args = _get_export_args(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        input_path=input_path,
        output_path=output_path,
    )
    subprocess.run(args, check=True)


def translate(
    workspace_root: str, genny_repo_root: str, input_path: str, output_path: Optional[str] = None
):
    args = _get_translate_args(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        input_path=input_path,
        output_path=output_path,
    )
    subprocess.run(args, check=True)


# Calculate rollups from all FTDC outputs.
def calculate_rollups(output_dir: str, workspace_root: str, genny_repo_root: str) -> None:
    curator = _find_curator(workspace_root, genny_repo_root)
    if not curator:
        raise OSError("Could not find Curator.")
    for root, dirs, files in os.walk(output_dir):
        for file in files:
            if file.endswith(".ftdc"):
                ftdc_file_name = os.path.join(root, file)

                # Curator produces invalid results for emtpy files.
                # We also need to remove the files to prevent the ftdc_fallback in DSI.
                if os.path.getsize(ftdc_file_name) == 0:
                    os.remove(ftdc_file_name)
                    continue

                rollup_file_name = ftdc_file_name.replace(".ftdc", ".json")
                SLOG.info(
                    "Creating perf rollup from FTDC file.",
                    ftdc_file=ftdc_file_name,
                    output=rollup_file_name,
                )
                args = [
                    curator,
                    "calculate-rollups",
                    "--inputFile",
                    ftdc_file_name,
                    "--outputFile",
                    rollup_file_name,
                ]
                subprocess.run(args, stderr=subprocess.STDOUT, text=True)


@contextmanager
def poplar_grpc(cleanup_metrics: bool, workspace_root: str, genny_repo_root: str):
    args = _get_poplar_args(workspace_root=workspace_root, genny_repo_root=genny_repo_root)

    if cleanup_metrics:
        _cleanup_metrics()
    _create_metrics()
    SLOG.info("Starting poplar grpc in the background.", command=args, cwd=workspace_root)

    prior_cwd = os.getcwd()
    try:
        os.chdir(workspace_root)
        with open("./build/WorkloadOutput/poplar_grpc.log", "w+") as log_file_obj:
            poplar = subprocess.Popen(
                args, stdout=log_file_obj, stderr=subprocess.STDOUT, text=True
            )
            poplar_port = int(os.environ.get("POPLAR_RPC_PORT", 2288))
            if poplar.poll() is not None:
                raise OSError("Failed to start Poplar.")
            try:
                os.chdir(prior_cwd)
                connected = False
                for i in range(10):
                    time.sleep(0.2)  # sleep to let curator get started. This is a heuristic.
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    try:
                        sock.connect(("127.0.0.1", poplar_port))
                        # If we didn't throw an exception, then we successfully connected
                        connected = True
                        break
                    except socket.error as socket_error:
                        if socket_error.errno != errno.ECONNREFUSED:
                            # We expect to get connection refused if poplar is not up yet.
                            # If we get something else, bail
                            raise socket_error
                    finally:
                        sock.close()
                if not connected:
                    raise OSError(f"Poplar not listening on port {poplar_port}.")
                yield poplar
            finally:
                try:
                    poplar.terminate()
                    exit_code = poplar.wait(timeout=10)
                    if exit_code not in (0, -15):  # Termination or exit.
                        raise OSError(f"Poplar exited with code: {exit_code}.")
                except:
                    # If Poplar doesn't die then future runs can be broken.
                    if poplar.poll() is None:
                        poplar.kill()
                    _report_poplar_error(log_file_obj)
                    raise
    finally:
        os.chdir(prior_cwd)


def _report_poplar_error(log: TextIO):
    """
    Review the log file for fatal errors and print them to stderr.

    We don't want to print the entire log, just highlight the fatal error(s).
    """

    log.seek(0)

    is_oom = False

    for line in log:
        line = line.strip().lower()
        if "fatal error" in line:
            # Error messages should go to sys.stderr. Especially, from the
            # perspective of DSI, all Genny SLOG log entries are just the stdout of
            # the child process regardless the log level.
            print(f"Poplar fatal error: {line}", file=sys.stderr)
            # Also, call SLOG in case we change SLOG to write to a log file in the
            # future.
            SLOG.error("Poplar fatal error", line=line)
        if "out of memory" in line or "cannot allocate memory" in line:
            #  such lines may appear multiple times. Don't print the prompt each
            #  time we see it
            is_oom = True

    if is_oom:
        print(
            "Poplar out of memory, please use a larger instance for the Genny workload client",
            file=sys.stderr,
        )


# For now we put curator in ./src/genny/build/curator, but ideally it would be in ./bin
# or in the python venv (if we make 'pip install curator' a thing).
def ensure_curator_installed(
    genny_repo_root: str, workspace_root: str, os_family: str, linux_distro: str
):
    install_dir = os.path.join(genny_repo_root, "build")
    os.makedirs(install_dir, exist_ok=True)
    downloader = CuratorDownloader(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        os_family=os_family,
        linux_distro=linux_distro,
        install_dir=install_dir,
    )
    downloader.fetch_and_install()


class CuratorDownloader(Downloader):
    # These build IDs are from the Curator Evergreen task.
    # https://evergreen.mongodb.com/waterfall/curator

    CURATOR_VERSION = "fa0dfec710dfbe2fb47c15d5800e978717f23614"

    DISTRO_MAPPING = {
        "archlinux": "linux-amd64",
        "amazon2": "rhel70",
        "rhel8": "rhel70",
        "rhel62": "rhel70",
        "ubuntu2004": "rhel70",
        "ubuntu2204": "rhel70",
        "amazon2_arm64": "arm",
        "ubuntu2004_arm64": "arm",
        "ubuntu2204_arm64": "arm",
        "amazon2023": "rhel70",
        "amazon2023_arm64": "arm",
    }

    def __init__(
        self,
        genny_repo_root: str,
        workspace_root: str,
        os_family: str,
        linux_distro: str,
        install_dir: str,
    ):
        super().__init__(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            os_family=os_family,
            linux_distro=linux_distro,
            install_dir=install_dir,
            name="curator",
        )
        self._curator_distro = self._linux_distro
        if self._os_family == "Darwin":
            self._curator_distro = "macos"

        # Note that this checks a substring. We could replace this check by finding all the valid
        # ubuntu versions and putting their names in DISTRO_MAPPING
        if "ubuntu" in self._linux_distro:
            self._curator_distro = "ubuntu"

        self._curator_distro = CuratorDownloader.DISTRO_MAPPING.get(
            self._linux_distro, self._curator_distro
        )

    def _get_url(self):
        version = CuratorDownloader.CURATOR_VERSION
        return (
            "https://s3.amazonaws.com/boxes.10gen.com/build/curator/"
            "curator-dist-{distro}-{build}.tar.gz".format(
                distro=self._curator_distro, build=version
            )
        )

    def _can_ignore(self):
        curator = _find_curator(
            workspace_root=self._workspace_root, genny_repo_root=self._genny_repo_root
        )
        if curator is None:
            return False
        res: RunCommandOutput = run_command(
            cmd=[curator, "-v"],
            check=True,
            cwd=self._workspace_root,
        )
        installed_version = "".join(res.stdout).strip()
        wanted_version = f"curator version {CuratorDownloader.CURATOR_VERSION}"
        SLOG.debug("Comparing curator versions", wanted=wanted_version, installed=installed_version)
        return installed_version == wanted_version
