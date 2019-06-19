import logging
import os
import shutil
import subprocess
import sys
import urllib.request

from context import Context


def get_toolchain_url(os_family, distro=None):
    # These build IDs are from the genny-toolchain Evergreen task.
    # https://evergreen.mongodb.com/waterfall/genny-toolchain
    if os_family == 'Darwin':
        distro = 'macos_1014'

    return 'https://s3.amazonaws.com/mciuploads/genny-toolchain/' \
           'genny_toolchain_{}_{}/gennytoolchain.tgz'.format(distro, Context.TOOLCHAIN_BUILD_ID)


def check_toolchain_githash(toolchain_dir):
    res = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=toolchain_dir, capture_output=True, text=True)
    return res.stdout.strip() == Context.TOOLCHAIN_GIT_HASH


def fetch_and_install_toolchain(url, install_dir):
    if not os.path.exists(install_dir):
        logging.critical(
            'Please create the parent directory for gennytoolchain: '
            '`sudo mkdir -p %s; sudo chown $USER %s`', install_dir, install_dir)
        sys.exit(1)

    if not os.access(install_dir, os.W_OK):
        logging.critical(
            'Please ensure you have write access to the parent directory for gennytoolchain: '
            '`sudo chown $USER %s`', install_dir)
        sys.exit(1)

    toolchain_dir = os.path.join(install_dir, 'gennytoolchain')
    # If the toolchain dir is outdated or we ignore the toolchain version.
    if os.path.exists(toolchain_dir) and (
            Context.IGNORE_TOOLCHAIN_VERSION or check_toolchain_githash(toolchain_dir)):
        logging.info('Skipping installing the toolchain into: %s', toolchain_dir)
    else:
        toolchain_tarball = os.path.join(install_dir, 'gennytoolchain.tgz')
        if os.path.isfile(toolchain_tarball):
            logging.info('Skipping downloading %s', toolchain_tarball)
        else:
            logging.info('Downloading gennytoolchain (>1GB), please wait...')
            urllib.request.urlretrieve(url, toolchain_tarball)
            logging.info('Finished Downloading gennytoolchain as %s', toolchain_tarball)

        logging.info('Extracting gennytoolchain into %s, please wait...', toolchain_dir)

        shutil.rmtree(toolchain_dir, ignore_errors=True)
        os.mkdir(toolchain_dir)
        # use tar(1) because python's TarFile was inexplicably truncating the tarball
        subprocess.run(['tar', '-xzf', toolchain_tarball, '-C', toolchain_dir], check=True)
        logging.info('Finished extracting gennytoolchain into %s', toolchain_dir)

        # Get 1GB back.
        os.remove(toolchain_tarball)

    return toolchain_dir
