import logging
import os

# Map of platform.system() to vcpkg's OS names.
_triplet_os_map = {
    'Darwin': 'osx',
    'Linux': 'linux',
    'NT': 'windows'
}


# Define complex operations as private methods on the module to keep the
# public Context object clean.
def _create_compile_environment(triplet_os, toolchain_dir):
    env = os.environ.copy()
    paths = [env['PATH']]

    # For mongodbtoolchain compiler (if there).
    paths.insert(0, '/opt/mongodbtoolchain/v3/bin')

    # For cmake and ctest
    cmake_bin_relative_dir = {
        'linux': 'downloads/tools/cmake-3.13.3-linux/cmake-3.13.3-Linux-x86_64/bin',
        'osx': 'downloads/tools/cmake-3.13.3-osx/cmake-3.13.3-Darwin-x86_64/CMake.app/Contents/bin'
    }[triplet_os]
    paths.insert(0, os.path.join(toolchain_dir, cmake_bin_relative_dir))

    # For ninja
    ninja_bin_dir = os.path.join(toolchain_dir,
                                 'downloads/tools/ninja-1.8.2-{}:'.format(triplet_os))
    paths.insert(0, ninja_bin_dir)

    env['PATH'] = ':'.join(paths)
    env['NINJA_STATUS'] = '[%f/%t (%p) %es] '  # make the ninja output even nicer

    logging.debug('Using environment: %s', env)
    return env


class Context:
    # Permanent constants.
    TOOLCHAIN_BUILD_ID = '564a715f940bdf6352ff76c4ab6ff3acd30f1d8c_19_06_04_19_13_24'
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split('_')[0]
    TOOLCHAIN_ROOT = '/data/mci'  # TODO BUILD-7624 change this to /opt.

    # Command line configurable and runtime values.
    IGNORE_TOOLCHAIN_VERSION = False
    TRIPLET_OS = None
    BUILD_SYSTEM = None
    SANITIZER = None

    # Dynamic context
    _compile_environment = None

    @staticmethod
    def get_compile_environment(toolchain_dir=None):
        if not Context._compile_environment:
            if not toolchain_dir:
                raise ValueError(
                    'toolchain_dir must be specified when getting the compile environment for the '
                    'first time')
            Context._compile_environment = _create_compile_environment(Context.TRIPLET_OS,
                                                                       toolchain_dir)
        return Context._compile_environment

    # Helper methods
    @staticmethod
    def set_triplet_os(os_family):
        Context.TRIPLET_OS = _triplet_os_map[os_family]
