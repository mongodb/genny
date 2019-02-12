import logging
import os


def _create_compile_environment(toolchain_dir, triplet_os):
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
    env['NINJA_STATUS'] = '[%f/%t (%p) %es]'  # make the ninja output even nicer

    logging.debug('Using environment: %s', env)
    return env


class Context:
    # Permanent constants.
    TOOLCHAIN_BUILD_ID = '7297534c3dc1df9f6d315098174171eaf75c5846_19_02_09_02_19_44'
    TOOLCHAIN_GIT_HASH = TOOLCHAIN_BUILD_ID.split('_')[0]

    # User configurable constants.
    IGNORE_TOOLCHAIN_VERSION = False

    # User configurable dynamic context.
    get_compile_environment = _create_compile_environment
