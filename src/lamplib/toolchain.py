from typing import Optional, NamedTuple

import json
import structlog
import os

from cmd_runner import run_command
from download import Downloader

SLOG = structlog.get_logger(__name__)

# Map of platform.system() to vcpkg's OS names.
_triplet_os_map = {"Darwin": "osx", "Linux": "linux", "NT": "windows"}


# Define complex operations as private methods on the module to keep the
# public Context object clean.
def _create_compile_environment(
    triplet_os: str, toolchain_dir: str, system_env: Optional[dict] = None
) -> dict:
    system_env = system_env if system_env else os.environ.copy()

    out = dict()
    paths = [system_env["PATH"]]

    # For mongodbtoolchain compiler (if there).
    paths.insert(0, "/opt/mongodbtoolchain/v3/bin")

    # For cmake and ctest
    cmake_bin_relative_dir = {
        "linux": "downloads/tools/cmake-3.13.3-linux/cmake-3.13.3-Linux-x86_64/bin",
        "osx": "downloads/tools/cmake-3.13.3-osx/cmake-3.13.3-Darwin-x86_64/CMake.app/Contents/bin",
    }[triplet_os]
    paths.insert(0, os.path.join(toolchain_dir, cmake_bin_relative_dir))

    # For ninja
    ninja_bin_dir = os.path.join(
        toolchain_dir, "downloads/tools/ninja-1.8.2-{}:".format(triplet_os)
    )
    paths.insert(0, ninja_bin_dir)

    out["PATH"] = ":".join(paths)
    out["NINJA_STATUS"] = "[%f/%t (%p) %es] "  # make the ninja output even nicer
    return out


class ToolchainInfo(NamedTuple):
    toolchain_dir: str
    triplet_os: str
    toolchain_env: dict
    linux_distro: str

    @property
    def is_darwin(self) -> bool:
        return self.triplet_os == "osx"

    def to_dict(self):
        return {
            "toolchain_dir": self.toolchain_dir,
            "triplet_os": self.triplet_os,
            "toolchain_env": self.toolchain_env,
            "linux_distro": self.linux_distro,
        }

    @staticmethod
    def from_dict(data):
        return ToolchainInfo(
            toolchain_dir=data["toolchain_dir"],
            triplet_os=data["triplet_os"],
            toolchain_env=data["toolchain_env"],
            linux_distro=data["linux_distro"],
        )


def _compute_toolchain_info(
    genny_repo_root: str,
    workspace_root: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
) -> ToolchainInfo:
    triplet_os = _triplet_os_map[os_family]
    toolchain_downloader = ToolchainDownloader(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
    toolchain_dir = toolchain_downloader.result_dir
    toolchain_env = _create_compile_environment(triplet_os, toolchain_dir)
    if not toolchain_downloader.fetch_and_install():
        raise Exception("Could not fetch and install")
    return ToolchainInfo(
        toolchain_dir=toolchain_dir,
        triplet_os=triplet_os,
        toolchain_env=toolchain_env,
        linux_distro=linux_distro,
    )


def toolchain_info(
    genny_repo_root: str,
    workspace_root: str,
    os_family: Optional[str] = None,
    linux_distro: Optional[str] = None,
    ignore_toolchain_version: Optional[bool] = None,
) -> ToolchainInfo:
    passed_args = [os_family, linux_distro, ignore_toolchain_version]
    passed_any = any(x for x in passed_args)

    save_path = os.path.join(genny_repo_root, "build", "ToolchainInfo.json")
    has_save = os.path.exists(save_path)

    SLOG.debug("Evaluating if need to recompute toolchain info", save_path=save_path,
               save_exists=has_save, passed_args=passed_args)

    if not passed_any and not has_save:
        msg = (
            f"You need to 'run-genny install' before running this command. "
            f"No toolchain info saved at {save_path}."
        )
        raise Exception(msg)
    if passed_any or not has_save:
        info: ToolchainInfo = _compute_toolchain_info(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            os_family=os_family,
            linux_distro=linux_distro,
            ignore_toolchain_version=ignore_toolchain_version,
        )
        with open(save_path, "w") as handle:
            json.dump(info.to_dict(), handle)
        SLOG.debug("Wrote toolchain info.", to_path=save_path)
    with open(save_path, "r") as handle:
        SLOG.debug("Loaded existing toolchain info", from_path=save_path)
        return ToolchainInfo.from_dict(json.load(handle))


class ToolchainDownloader(Downloader):
    # These build IDs are from the genny-toolchain Evergreen task.
    # https://evergreen.mongodb.com/waterfall/genny-toolchain

    TOOLCHAIN_BUILD_ID = "0db5d1544746c1570371f51109a0a312a7215b65_20_10_01_06_38_39"
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split("_")[0]
    TOOLCHAIN_ROOT = "/data/mci"  # TODO BUILD-7624 change this to /opt.

    def __init__(
        self,
        genny_repo_root: str,
        workspace_root: str,
        os_family: str,
        linux_distro: str,
        ignore_toolchain_version: bool,
    ):
        super().__init__(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            os_family=os_family,
            linux_distro=linux_distro,
            install_dir=ToolchainDownloader.TOOLCHAIN_ROOT,
            name="gennytoolchain",
        )
        self.ignore_toolchain_version = ignore_toolchain_version

    def _get_url(self):
        prefix = "macos_1014" if self._os_family == "Darwin" else self._linux_distro
        return (
            "https://s3.amazonaws.com/mciuploads/genny-toolchain/"
            "genny_toolchain_{}_{}/gennytoolchain.tgz".format(
                prefix, ToolchainDownloader.TOOLCHAIN_BUILD_ID
            )
        )

    def _can_ignore(self):
        # If the toolchain dir is outdated or we ignore the toolchain version.
        return os.path.exists(self.result_dir) and (
            self.ignore_toolchain_version or self._check_toolchain_githash()
        )

    def _check_toolchain_githash(self):
        res = "".join(
            run_command(cmd=["git", "rev-parse", "HEAD"], cwd=self.result_dir, check=True).stdout
        )
        return res.strip() == ToolchainDownloader.TOOLCHAIN_GIT_HASH
