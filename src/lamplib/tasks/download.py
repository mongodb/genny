import logging
import os
import shutil
import subprocess
import sys
import urllib.request

from context import Context

class Downloader():
    """
    Abstract base class for things that can be downloaded.

    :return: The directory the target was installed to.
    """

    def __init__(self, os_family, distro, install_dir, name):
        self._os_family = os_family
        self._distro = distro
        self._name = name

        # Install dir is the directory we create the actual result directory in.
        self._install_dir = install_dir
        self.result_dir = None

    def fetch_and_install(self):
        """
        Download the target and install to its directory.

        :return: Whether the operation succeeded.
        """
        if not os.path.exists(self._install_dir):
            logging.critical(
                'Please create the parent directory for %s: '
                '`sudo mkdir -p %s; sudo chown $USER %s`', self._name, self._install_dir, self._install_dir)
            return False 

        if not os.access(self._install_dir, os.W_OK):
            logging.critical(
                'Please ensure you have write access to the parent directory for %s: '
                '`sudo chown $USER %s`', self._name, self._install_dir)
            return False

        self.result_dir = os.path.join(self._install_dir, self._name)
        if self._can_ignore():
            logging.info('Skipping installing the %s into: %s', self._name, self.result_dir)
        else:
            tarball = os.path.join(self._install_dir, self._name + '.tgz')
            if os.path.isfile(tarball):
                logging.info('Skipping downloading %s', tarball)
            else:
                logging.info('Downloading gennytoolchain (>1GB), please wait...')
                url = self._get_url()
                urllib.request.urlretrieve(url, tarball)
                logging.info('Finished Downloading gennytoolchain as %s', tarball)

            logging.info('Extracting gennytoolchain into %s, please wait...', self.result_dir)

            shutil.rmtree(self.result_dir, ignore_errors=True)
            os.mkdir(self.result_dir)
            # use tar(1) because python's TarFile was inexplicably truncating the tarball
            subprocess.run(['tar', '-xzf', tarball, '-C', self.result_dir], check=True)
            logging.info('Finished extracting %s into %s', self._name, self.result_dir)

            # Get space back.
            os.remove(tarball)

        return True


    def _get_url(self):
        raise NotImplementedError

    def _can_ignore(self):
        raise NotImplementedError

class ToolchainDownloader(Downloader):

    def __init__(self, os_family, distro):
        super().__init__(os_family, distro, Context.TOOLCHAIN_ROOT, "gennytoolchain")

    def _get_url(self):
        # These build IDs are from the genny-toolchain Evergreen task.
        # https://evergreen.mongodb.com/waterfall/genny-toolchain
        if self._os_family == 'Darwin':
            self._distro = 'macos_1014'

        return 'https://s3.amazonaws.com/mciuploads/genny-toolchain/' \
               'genny_toolchain_{}_{}/gennytoolchain.tgz'.format(self._distro, Context.TOOLCHAIN_BUILD_ID)

    def _can_ignore(self):
        # If the toolchain dir is outdated or we ignore the toolchain version.
        return os.path.exists(self.result_dir) and (Context.IGNORE_TOOLCHAIN_VERSION or _check_toolchain_githash(self.result_dir))

def _check_toolchain_githash(toolchain_dir):
    res = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=toolchain_dir, capture_output=True, text=True)
    return res.stdout.strip() == Context.TOOLCHAIN_GIT_HASH
