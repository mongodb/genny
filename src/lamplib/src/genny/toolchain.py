import json
import os
import platform

import structlog
from typing import Optional, NamedTuple

from genny.cmd_runner import run_command
from genny.download import Downloader


SLOG = structlog.get_logger(__name__)

# Map of platform.system() to vcpkg's OS names.
_triplet_os_map = {"Darwin": "osx", "Linux": "linux", "NT": "windows"}


# Define complex operations as private methods on the module to keep the
# public Context object clean.
def _create_compile_environment(
    triplet_os: str, toolchain_dir: str, triplet_arch: str, system_env: Optional[dict] = None
) -> dict:
    system_env = system_env if system_env else os.environ.copy()

    out = dict()
    paths = [system_env["PATH"]]

    # For mongodbtoolchain compiler (if there).
    paths.insert(0, "/opt/mongodbtoolchain/v4/bin")
    # But use our toolchain's ninja
    paths.insert(
        0, os.path.join(toolchain_dir, f"installed/{triplet_arch}-{triplet_os}-release/tools/ninja")
    )

    out["PATH"] = ":".join(paths)
    out["NINJA_STATUS"] = "[%f/%t (%p) %es] "  # make the ninja output even nicer
    return out


class ToolchainInfo(NamedTuple):
    toolchain_dir: str
    triplet_os: str
    toolchain_env: dict
    linux_distro: str
    triplet_arch: str

    @property
    def is_darwin(self) -> bool:
        return self.triplet_os == "osx"

    def to_dict(self):
        return {
            "toolchain_dir": self.toolchain_dir,
            "triplet_os": self.triplet_os,
            "toolchain_env": self.toolchain_env,
            "linux_distro": self.linux_distro,
            "triplet_arch": self.triplet_arch,
        }

    @staticmethod
    def from_dict(data):
        return ToolchainInfo(
            toolchain_dir=data["toolchain_dir"],
            triplet_os=data["triplet_os"],
            toolchain_env=data["toolchain_env"],
            linux_distro=data["linux_distro"],
            triplet_arch=data["triplet_arch"],
        )


def _compute_toolchain_info(
    genny_repo_root: str,
    workspace_root: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
) -> ToolchainInfo:
    triplet_arch = "x64"
    if len(linux_distro) > 6 and linux_distro[-6:] == "_arm64":
        # assumes all arm64 linux distros have this suffix, e.g. 'ubuntu2204_arm64'
        triplet_arch = "arm64"
    if os_family == "Darwin":
        triplet_arch = "arm64" if platform.processor() == "arm" else "x64"

    if os_family not in _triplet_os_map:
        raise Exception(f"os_family {os_family} is unknown. Pass the --linux-distro option.")
    triplet_os = _triplet_os_map[os_family]
    toolchain_downloader = ToolchainDownloader(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
        triplet_arch=triplet_arch,
    )
    toolchain_dir = toolchain_downloader.result_dir
    toolchain_env = _create_compile_environment(triplet_os, toolchain_dir, triplet_arch)
    if not toolchain_downloader.fetch_and_install():
        raise Exception("Could not fetch and install")
    return ToolchainInfo(
        toolchain_dir=toolchain_dir,
        triplet_os=triplet_os,
        triplet_arch=triplet_arch,
        toolchain_env=toolchain_env,
        linux_distro=linux_distro,
    )


def toolchain_info(
    genny_repo_root: str,
    workspace_root: str,
    triplet_arch: Optional[str] = None,
    os_family: Optional[str] = None,
    linux_distro: Optional[str] = None,
    ignore_toolchain_version: Optional[bool] = None,
) -> ToolchainInfo:
    passed_args = [os_family, linux_distro, ignore_toolchain_version]
    passed_any = any(x for x in passed_args)

    save_path = os.path.join(genny_repo_root, "build", "ToolchainInfo.json")
    has_save = os.path.exists(save_path)

    SLOG.debug(
        "Evaluating if need to recompute toolchain info",
        save_path=save_path,
        save_exists=has_save,
        passed_args=passed_args,
        passed_any=passed_any,
    )

    if not passed_any and not has_save:
        msg = (
            f"You need to 'run-genny install' before running this command. "
            f"No toolchain info saved at {save_path}."
        )
        raise Exception(msg)
    if passed_any or not has_save:
        SLOG.debug(
            "Passed build args or no saved toolchain info",
            passed_any=passed_any,
            has_save=has_save,
            save_path=save_path,
        )
        info: ToolchainInfo = _compute_toolchain_info(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            os_family=os_family,
            linux_distro=linux_distro,
            ignore_toolchain_version=ignore_toolchain_version,
        )
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        with open(save_path, "w") as handle:
            json.dump(info.to_dict(), handle)
            SLOG.debug("Wrote toolchain info.", to_path=save_path)
    if not os.path.exists(save_path):
        raise Exception(f"Saved toolchain path {save_path} does not exist.")
    with open(save_path, "r") as handle:
        SLOG.debug("Loaded existing toolchain info", from_path=save_path)
        return ToolchainInfo.from_dict(json.load(handle))


class ToolchainDownloader(Downloader):
    # These build IDs are from the genny-toolchain Evergreen task (which pulls from 10gen/vcpkg repo)
    # Old UI: https://evergreen.mongodb.com/waterfall/genny-toolchain
    # New UI: https://spruce.mongodb.com/commits/genny-toolchain
    #
    # Find a compile task (for any build variant) and get TOOLCHAIN_BUILD_ID from URL
    # https://spruce.mongodb.com/task/genny_toolchain_<$build-variant>_t_compile_<$TOOLCHAIN_BUILD_ID>
    # Example for merged PR build on archlinux variant:
    # https://spruce.mongodb.com/task/genny_toolchain_archlinux_t_compile_82eb7c32ad09726f3ef0ddc8d7f24a18b03d9644_21_11_23_16_37_21
    # =>                                                                  82eb7c32ad09726f3ef0ddc8d7f24a18b03d9644_21_11_23_16_37_21
    # Example for patch build on macos_1100_arm64 variant:
    # https://spruce.mongodb.com/task/genny_toolchain_macos_1100_arm64_t_compile_patch_87457e6fec1d98f270c84d915f83bec53554ecee_6451d23fc9ec4441c9ce233d_23_05_03_03_17_20
    # =>                                                                         patch_87457e6fec1d98f270c84d915f83bec53554ecee_6451d23fc9ec4441c9ce233d_23_05_03_03_17_20
    # If we were 💅 we could do the string logic here in python, but we're not that fancy.
    #

    TOOLCHAIN_BUILD_ID = (
        "e5a0c94080f72e00a20ba8fd6ca6a3082e0873a6_24_05_30_06_59_47"
    )
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split("_")[0]

    def __init__(
        self,
        genny_repo_root: str,
        workspace_root: str,
        os_family: str,
        linux_distro: str,
        triplet_arch: str,
        ignore_toolchain_version: bool,
    ):
        running_in_evergreen = "EVR_TASK_ID" in os.environ

        # Install the toolchain in /opt on OS X on dev laptp[s] so we don't
        # have to ask users to do crazy things to get "/data" to work.
        # Evergreen OS X machines don't have this problem
        toolchain_root = (
            "/opt/data/mci" if os_family == "Darwin" and not running_in_evergreen else "/data/mci"
        )

        super().__init__(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            os_family=os_family,
            linux_distro=linux_distro,
            install_dir=toolchain_root,
            name="gennytoolchain",
        )
        self.ignore_toolchain_version = ignore_toolchain_version
        self.triplet_arch = triplet_arch

    def _get_url(self):
        # TODO: Need to update prefixes for waterfall
        if self._os_family == "Darwin":
            if self.triplet_arch == "arm64":
                prefix = "macos_1100_arm64"
            else:
                prefix = "macos_1100"
        else:
            prefix = self._linux_distro
        return (
            "https://s3.amazonaws.com/mciuploads/genny-toolchain/"
            "genny_toolchain_{}_{}/gennytoolchain.tgz".format(
                prefix, ToolchainDownloader.TOOLCHAIN_BUILD_ID
            )
        )

    def _can_ignore(self):
        # If the toolchain dir is outdated, or we ignore the toolchain version.
        return os.path.exists(self.result_dir) and (
            self.ignore_toolchain_version or self._check_toolchain_githash()
        )

    def _check_toolchain_githash(self):
        res = "".join(
            run_command(cmd=["git", "rev-parse", "HEAD"], cwd=self.result_dir, check=True).stdout
        )
        return res.strip() == ToolchainDownloader.TOOLCHAIN_GIT_HASH
