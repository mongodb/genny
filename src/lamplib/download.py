import structlog
import os
import shutil
import platform
import urllib.request

from cmd_runner import run_command

SLOG = structlog.get_logger(__name__)


class Downloader:
    """
    Abstract base class for things that can be downloaded.

    :return: The directory the target was installed to.
    """

    def __init__(self, os_family: str, distro: str, install_dir: str, name: str):
        """
        :param install_dir: the directory the target will be installed to
        :param name: name of the target being downloaded
        """
        self._os_family = os_family
        self._distro = distro
        self._name = name

        # Install dir is the directory we create the actual result in.
        self._install_dir = install_dir
        self.result_dir = os.path.join(self._install_dir, self._name)

    def fetch_and_install(self):
        """
        Download the target and install to its directory.

        :return: Whether the operation succeeded.
        """
        if self._can_ignore():
            SLOG.debug("Skipping installing", name=self._name, into_dir=self.result_dir)
            return True

        if self._can_install():
            self._fetch_and_install_impl()
            return True
        return False

    def _can_install(self):
        okay = True
        if not os.path.exists(self._install_dir):
            try:
                os.makedirs(self._install_dir)
            except OSError:
                SLOG.critical(
                    "Please create the parent directory.",
                    trying_to_install=self._name,
                    install_dir_doesnt_exist=self._install_dir,
                )
                okay = False

        if not os.path.isdir(self._install_dir):
            SLOG.critical("Install dir is not a directory.", what_is_not_a_dir=self._install_dir)
            okay = False

        if not os.access(self._install_dir, os.W_OK):
            SLOG.critical(
                "Please ensure you have write access:" f'`sudo chown $USER "{self._install_dir}"`',
            )
            okay = False

        if not okay and platform.mac_ver()[0]:
            release_triplet = platform.mac_ver()[0].split(".")
            minor_ver = int(release_triplet[1])
            if 12 <= minor_ver < 15:
                SLOG.info(
                    "On versions of MacOS between 10.12 and 10.14, "
                    "you have to disable System Integrity Protection first. "
                    "See https://apple.stackexchange.com/a/208481 for instructions",
                    macos_minor_version=minor_ver,
                )
            if minor_ver >= 15:
                SLOG.info(_macos_install_instructions(self._name))

        return okay

    def _fetch_and_install_impl(self):
        tarball = os.path.join(self._install_dir, self._name + ".tgz")
        if os.path.isfile(tarball):
            SLOG.info("Skipping downloading since already exists", tarball=tarball)
        else:
            url = self._get_url()
            SLOG.debug("Downloading", name=self._name, url=url)
            urllib.request.urlretrieve(url, tarball)
            SLOG.debug("Finished Downloading", name=self._name, tarball=tarball)

        SLOG.debug("Extracting", name=self._name, into=self.result_dir)

        shutil.rmtree(self.result_dir, ignore_errors=True)
        os.makedirs(self.result_dir, exist_ok=True)
        # use tar(1) because python's TarFile was inexplicably truncating the tarball
        run_command(cmd=["tar", "-xzf", tarball, "-C", self.result_dir], capture=False)
        SLOG.info("Downloaded and installed.", name=self._name, into=self.result_dir)

        # Get space back.
        os.remove(tarball)

    def _get_url(self):
        raise NotImplementedError

    def _can_ignore(self):
        raise NotImplementedError


# Instructions derived from https://github.com/NixOS/nix/issues/2925#issuecomment-539570232
def _macos_install_instructions(name):
    return fr"""

😲 You must create the parent directory {name} for the genny toolchain.
   You are on On MacOS Catalina or later, so use use the synthetic.conf method.

We wish we didn't have to do this.

1️⃣  Step 1 1️⃣
Run the followng command

    echo 'data' | sudo tee -a /etc/synthetic.conf


2️⃣  Step 2 2️⃣

    🚨🚨 Restart your system before continuing. 🚨🚨
                Really. Don't skip this.

Once restarted, run the lamp command again to show this message again.


3️⃣  Step 3 3️⃣
Determine which of your local disks is the "synthesized" APFS volume.
Run `diskutil list` and look for the line with "(synthesized)".

For example:

    $ diskutil list
    /dev/disk0 (internal, physical):
       #:                       TYPE NAME                    SIZE       IDENTIFIER
       0:      GUID_partition_scheme                        *1.0 TB     disk0
       1:                        EFI EFI                     314.6 MB   disk0s1
       2:                 Apple_APFS Container disk1         1.0 TB     disk0s2

    /dev/disk1 (synthesized): 💥💥💥
       #:                       TYPE NAME                    SIZE       IDENTIFIER
       0:      APFS Container Scheme -                      +1.0 TB     disk1
                                     Physical Store disk0s2
       1:                APFS Volume Macintosh HD            11.0 GB    disk1s1
       2:                APFS Volume Macintosh HD - Data     519.6 GB   disk1s2
       3:                APFS Volume Preboot                 82.3 MB    disk1s3
       4:                APFS Volume Recovery                528.5 MB   disk1s4
       5:                APFS Volume VM                      5.4 GB     disk1s5

    /dev/disk2 (disk image):
       #:                       TYPE NAME                    SIZE       IDENTIFIER
       0:      GUID_partition_scheme                        +1.9 TB     disk2
       1:                        EFI EFI                     209.7 MB   disk2s1
       2:                  Apple_HFS Time Machine Backups    1.9 TB     disk2s2

In this example, the synthesized disk is disk1. Use that in Step 4.

4️⃣  Step 4 4️⃣
Then run the following commands:

    Did you *actually* restart your computer after running Step 1?
        This won't work if you didn't.
    Replace disk1 with the synthesized disk from Step 3 if necessary.

    $ sudo diskutil apfs addVolume disk1 APFSX Data -mountpoint /data 
    $ sudo diskutil enableOwnership /data 
    $ sudo chflags hidden /data 
    $ echo \LABEL=Data /data apfs rw\ | sudo tee -a /etc/fstab 
    $ mkdir /data/mci

👯‍♂️🧞‍♀️ Back to real life 🧞‍♂️👯
Re-run the lamp command to download and setup the genny toolchain and build genny.


☝️ There are some steps you have to before you can build and run genny. Scroll up. ☝️"""
