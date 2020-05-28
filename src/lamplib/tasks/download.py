import logging
import os
import shutil
import subprocess
import sys
import platform
import urllib.request

from context import Context


class Downloader:
    """
    Abstract base class for things that can be downloaded.

    :return: The directory the target was installed to.
    """

    def __init__(self, os_family, distro, install_dir, name):
        """
        :param os_family: string
        :param distro: string
        :param install_dir: the directory the target will be installed to
        :param name: name of the target being downloaded
        """
        self._os_family = os_family
        self._distro = distro
        self._name = name

        # Install dir is the directory we create the actual result in.
        self._install_dir = install_dir
        self.result_dir = None

    def fetch_and_install(self):
        """
        Download the target and install to its directory.

        :return: Whether the operation succeeded.
        """
        if not os.path.exists(self._install_dir):
            logging.critical(
                "Please create the parent directory for %s.", self._name
            )

            if platform.mac_ver()[0]:
                release_triplet = platform.mac_ver()[0].split(".")
                minor_ver = int(release_triplet[1])
                if minor_ver >= 12 and minor_ver < 15:
                    logging.info(
                        "On versions of MacOS between 10.12 and 10.14, "
                        "you have to disable System Integrity Protection first. "
                        "See https://apple.stackexchange.com/a/208481 for instructions")
                if minor_ver >= 15:
                    # Instructions derived from https://github.com/NixOS/nix/issues/2925#issuecomment-539570232
                    logging.info(
                        "On MacOS Catalina and later, use synthetic.conf. "
                        "The following instructions assume that disk1 is the synthesized AFPS volume of the primary, "
                        "internal volume your Mac. Verify this assumption with `diskutil list`, substituting the "
                        "appropriate volume name if required. \n\n"

                        "echo 'data' | sudo tee -a /etc/synthetic.conf\n"
                        "*Restart your system before continuing*\n\n"

                        "sudo diskutil apfs addVolume disk1 APFSX Data -mountpoint /data \\\n"
                        "sudo diskutil enableOwnership /data \\\n"
                        "sudo chflags hidden /data \\\n"
                        "echo \"LABEL=Data /data apfs rw\" | sudo tee -a /etc/fstab \\\n"
                        "mkdir /data/mci"
                        )
                    return False

            logging.critical('`sudo mkdir -p "%s"; sudo chown "$USER" "%s"`',
                self._install_dir,
                self._install_dir,
            )

            return False

        if not os.path.isdir(self._install_dir):
            logging.critical("Install dir %s is not a directory.", self._install_dir)
            return False

        if not os.access(self._install_dir, os.W_OK):
            logging.critical(
                "Please ensure you have write access to the parent directory for %s: "
                "`sudo chown $USER %s`",
                self._name,
                self._install_dir,
            )
            return False

        self.result_dir = os.path.join(self._install_dir, self._name)
        if self._can_ignore():
            logging.debug("Skipping installing the %s into: %s", self._name, self.result_dir)
        else:
            tarball = os.path.join(self._install_dir, self._name + ".tgz")
            if os.path.isfile(tarball):
                logging.info("Skipping downloading %s", tarball)
            else:
                logging.info("Downloading %s, please wait...", self._name)
                url = self._get_url()
                urllib.request.urlretrieve(url, tarball)
                logging.info("Finished Downloading %s as %s", self._name, tarball)

            logging.info("Extracting %s into %s, please wait...", self._name, self.result_dir)

            shutil.rmtree(self.result_dir, ignore_errors=True)
            os.mkdir(self.result_dir)
            # use tar(1) because python's TarFile was inexplicably truncating the tarball
            subprocess.run(["tar", "-xzf", tarball, "-C", self.result_dir], check=True)
            logging.info("Finished extracting %s into %s", self._name, self.result_dir)

            # Get space back.
            os.remove(tarball)

        return True

    def _get_url(self):
        raise NotImplementedError

    def _can_ignore(self):
        raise NotImplementedError


class ToolchainDownloader(Downloader):
    # These build IDs are from the genny-toolchain Evergreen task.
    # https://evergreen.mongodb.com/waterfall/genny-toolchain

    TOOLCHAIN_BUILD_ID = "f26aecf5527c3abdc2ad67e740050bf1f5ae8149_20_05_19_16_54_27"
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split("_")[0]
    TOOLCHAIN_ROOT = "/data/mci"  # TODO BUILD-7624 change this to /opt.

    def __init__(self, os_family, distro):
        super().__init__(os_family, distro, ToolchainDownloader.TOOLCHAIN_ROOT, "gennytoolchain")

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
            Context.IGNORE_TOOLCHAIN_VERSION or self._check_toolchain_githash()
        )

    def _check_toolchain_githash(self):
        res = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=self.result_dir, capture_output=True, text=True
        )
        return res.stdout.strip() == ToolchainDownloader.TOOLCHAIN_GIT_HASH


class CuratorDownloader(Downloader):
    # These build IDs are from the Curator Evergreen task.
    # https://evergreen.mongodb.com/waterfall/curator

    # Note that DSI also downloads Curator, the location is specified in defaults.yml.
    # Please try to keep the two versions consistent.
    CURATOR_VERSION = "cd30712870fa84767e6dc3559c9a1ec9ac8e654f"
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
