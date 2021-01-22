import structlog
import os

from cmd_runner import run_command
from download import Downloader

SLOG = structlog.get_logger(__name__)


# Map of platform.system() to vcpkg's OS names.
_triplet_os_map = {"Darwin": "osx", "Linux": "linux", "NT": "windows"}


# Define complex operations as private methods on the module to keep the
# public Context object clean.
def _create_compile_environment(triplet_os, toolchain_dir):
    env = os.environ.copy()
    paths = [env["PATH"]]

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

    env["PATH"] = ":".join(paths)
    env["NINJA_STATUS"] = "[%f/%t (%p) %es] "  # make the ninja output even nicer
    return env


def toolchain_info(os_family: str, linux_distro: str, ignore_toolchain_version: bool):
    triplet_os = _triplet_os_map[os_family]
    toolchain_downloader = ToolchainDownloader(os_family, linux_distro, ignore_toolchain_version)
    toolchain_dir = toolchain_downloader.result_dir
    toolchain_env = _create_compile_environment(triplet_os, toolchain_dir)
    if not toolchain_downloader.fetch_and_install():
        raise Exception("Could not fetch and install")
    return {
        "toolchain_dir": toolchain_dir,
        "triplet_os": triplet_os,
        "toolchain_env": toolchain_env,
    }


class ToolchainDownloader(Downloader):
    # These build IDs are from the genny-toolchain Evergreen task.
    # https://evergreen.mongodb.com/waterfall/genny-toolchain

    TOOLCHAIN_BUILD_ID = "0db5d1544746c1570371f51109a0a312a7215b65_20_10_01_06_38_39"
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split("_")[0]
    TOOLCHAIN_ROOT = "/data/mci"  # TODO BUILD-7624 change this to /opt.

    def __init__(self, os_family, distro, ignore_toolchain_version: bool):
        super().__init__(os_family, distro, ToolchainDownloader.TOOLCHAIN_ROOT, "gennytoolchain")
        self.ignore_toolchain_version = ignore_toolchain_version

    def _get_url(self):
        if self._os_family == "Darwin":
            self._distro = "macos_1014"

        return (
            "https://s3.amazonaws.com/mciuploads/genny-toolchain/"
            "genny_toolchain_{}_{}/gennytoolchain.tgz".format(
                self._distro, ToolchainDownloader.TOOLCHAIN_BUILD_ID
            )
        )

    def _can_ignore(self):
        # If the toolchain dir is outdated or we ignore the toolchain version.
        return os.path.exists(self.result_dir) and (
            self.ignore_toolchain_version or self._check_toolchain_githash()
        )

    def _check_toolchain_githash(self):
        res = "".join(run_command(cmd=["git", "rev-parse", "HEAD"], cwd=self.result_dir))
        return res.strip() == ToolchainDownloader.TOOLCHAIN_GIT_HASH
