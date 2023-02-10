import structlog
import os
import shutil
import platform
import urllib.request
import progressbar

from genny.cmd_runner import run_command

SLOG = structlog.get_logger(__name__)


class ProgressBar:
    """
    A simple progress bar to show while downloading the toolkit tarball. Useful to let the user know that the
     application hasn't frozen.
    """

    def __init__(self):
        self.bar = None

    def __call__(self, block_num, block_size, total_size):
        if not self.bar:
            self.bar = progressbar.ProgressBar(maxval=total_size)
            self.bar.start()

        downloaded = block_num * block_size
        if downloaded < total_size:
            self.bar.update(downloaded)
        else:
            self.bar.finish()


class Downloader:
    """
    Abstract base class for things that can be downloaded.

    :return: The directory the target was installed to.
    """

    def __init__(
        self,
        genny_repo_root: str,
        workspace_root: str,
        os_family: str,
        linux_distro: str,
        install_dir: str,
        name: str,
    ):
        """
        :param install_dir: the directory the target will be installed to
        :param name: name of the target being downloaded
        """
        self._genny_repo_root = genny_repo_root
        self._workspace_root = workspace_root

        self._os_family = os_family
        self._linux_distro = linux_distro
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

        return okay

    def _fetch_and_install_impl(self) -> None:
        tarball = os.path.join(self._install_dir, self._name + ".tgz")
        # if os.path.isfile(tarball):
        #    SLOG.info("Skipping downloading since already exists", tarball=tarball)
        # else:
        url = self._get_url()
        SLOG.debug("Downloading", name=self._name, url=url)
        print(f"Downloading {url} to {tarball}")
        urllib.request.urlretrieve(url, tarball, ProgressBar())
        SLOG.debug("Finished Downloading", name=self._name, tarball=tarball)

        SLOG.debug("Extracting", name=self._name, into=self.result_dir)

        shutil.rmtree(self.result_dir, ignore_errors=True)
        os.makedirs(self.result_dir, exist_ok=True)
        # use tar(1) because python's TarFile was inexplicably truncating the tarball
        run_command(
            cmd=["tar", "-xzf", tarball, "-C", self.result_dir],
            capture=False,
            check=True,
            # The cwd doesn't actually matter here since we do 'tar -C'
            cwd=self._workspace_root,
        )
        SLOG.info("Downloaded and installed.", name=self._name, into=self.result_dir)

        # Get space back.
        os.remove(tarball)

    def _get_url(self):
        raise NotImplementedError

    def _can_ignore(self):
        raise NotImplementedError
