import os
import shutil
import subprocess
from contextlib import contextmanager

from download import Downloader


def get_poplar_args():
    """
    Returns the argument list used to create the Poplar gRPC process.

    If we are in the root of the genny repo, use the local executable.
    Otherwise we search the PATH.
    """
    curator = "curator"

    # TODO: look on PATH and download if not exists.
    local_curator = "./curator/curator"
    if os.path.exists(local_curator):
        curator = local_curator
    return [curator, "poplar", "grpc"]


@contextmanager
def poplar_grpc(cleanup=False):
    poplar = subprocess.Popen(get_poplar_args())
    if poplar.poll() is not None:
        raise OSError("Failed to start Poplar.")

    try:
        yield poplar
    finally:
        try:
            poplar.terminate()
            exit_code = poplar.wait(timeout=10)
            if exit_code not in (0, -15):  # Termination or exit.
                raise OSError("Poplar exited with code: {code}.".format(code=(exit_code)))

        except:
            # If Poplar doesn't die then future runs can be broken.
            if poplar.poll() is None:
                poplar.kill()
            raise
        finally:
            if cleanup:
                shutil.rmtree("build/CedarMetrics", ignore_errors=True)


class CuratorDownloader(Downloader):
    # These build IDs are from the Curator Evergreen task.
    # https://evergreen.mongodb.com/waterfall/curator

    # Note that DSI also downloads Curator, the location is specified in defaults.yml.
    # Please try to keep the two versions consistent.
    CURATOR_VERSION = "d3da25b63141aa192c5ef51b7d4f34e2f3fc3880"
    CURATOR_ROOT = os.getcwd()

    def __init__(self, os_family, distro):
        super().__init__(os_family, distro, CuratorDownloader.CURATOR_ROOT, "curator")

    def _get_url(self):
        if self._os_family == "Darwin":
            self._distro = "macos"

        if "ubuntu" in self._distro:
            self._distro = "ubuntu1604"

        if self._distro in ("amazon2", "rhel8", "rhel62"):
            self._distro = "rhel70"

        return (
            "https://s3.amazonaws.com/boxes.10gen.com/build/curator/"
            "curator-dist-{distro}-{build}.tar.gz".format(
                distro=self._distro, build=CuratorDownloader.CURATOR_VERSION
            )
        )

    def _can_ignore(self):
        return os.path.exists(self.result_dir) and (
            Context.IGNORE_TOOLCHAIN_VERSION or self._check_curator_version()
        )

    def _check_curator_version(self):
        res = subprocess.run(
            ["./curator", "-v"], cwd=self.result_dir, capture_output=True, text=True
        )
        return res.stdout.split()[2] == CuratorDownloader.CURATOR_VERSION
