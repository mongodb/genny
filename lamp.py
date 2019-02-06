#!/usr/bin/env python3

from __future__ import print_function

import argparse
import logging
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request


def get_toolchain_url(os_family, distro=None):
    if os_family == 'Darwin':
        build_id = 'genny_toolchain_macos_1014_b15f51760a9e651fb66bc0593e2eea738e15ffa2_19_02_07_07_04_06'
    elif os_family == 'Linux':
        if distro == 'ubuntu1804':
            build_id = 'genny_toolchain_ubuntu1804_b15f51760a9e651fb66bc0593e2eea738e15ffa2_19_02_07_07_04_06'
        elif distro == 'rhel7':
            build_id = 'genny_toolchain_rhel70_b15f51760a9e651fb66bc0593e2eea738e15ffa2_19_02_07_07_04_06'
        elif distro == 'amazon2':
            build_id = 'genny_toolchain_amazon2_b15f51760a9e651fb66bc0593e2eea738e15ffa2_19_02_07_07_04_06'
        elif distro == 'arch':
            build_id = 'genny_toolchain_archlinux_b15f51760a9e651fb66bc0593e2eea738e15ffa2_19_02_07_07_04_06'
        else:
            raise ValueError('Unknown Linux distro {}; please set "--linux-distro"'.format(distro))
    else:
        raise ValueError('Unsupported platform: {}'.format(os_family))

    url = 'https://s3.amazonaws.com/mciuploads/genny-toolchain/{}/gennytoolchain.tgz'.format(
        build_id)
    logging.debug('gennytoolchain URL: %s', url)
    return url


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

    toolchain_tarball = os.path.join(install_dir, 'gennytoolchain.tgz')
    if os.path.isfile(toolchain_tarball):
        logging.info('Skipping downloading %s', toolchain_tarball)
    else:
        logging.info('Downloading gennytoolchain, please wait...')
        urllib.request.urlretrieve(url, toolchain_tarball)
        logging.info('Finished Downloading gennytoolchain as %s', toolchain_tarball)

    toolchain_dir = os.path.join(install_dir, 'gennytoolchain')
    if os.path.exists(toolchain_dir):
        logging.info('Skipping extracting the toolchain into: %s', toolchain_dir)
    else:
        logging.info('Extracting gennytoolchain, please wait...')
        tarball = tarfile.open(toolchain_tarball)
        tarball.extractall(toolchain_dir)
        logging.info('Finished extracting gennytoolchain into %s', toolchain_dir)

    return toolchain_dir


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


def cmake(toolchain_dir, cmdline_cmake_args, triplet_os, env):
    cmake_cmd = ['cmake', '-B', 'build', '-G', 'Ninja']
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(toolchain_dir, 'installed/x64-{}-shared'.format(triplet_os))
    cmake_toolchain_file = os.path.join(toolchain_dir, 'scripts/buildsystems/vcpkg.cmake')

    cmake_cmd += [
        '-DCMAKE_PREFIX_PATH={}'.format(cmake_prefix_path),
        '-DCMAKE_TOOLCHAIN_FILE={}'.format(cmake_toolchain_file),
        '-DVCPKG_TARGET_TRIPLET=x64-{}-static'.format(triplet_os),
    ]

    cmake_cmd += cmdline_cmake_args

    logging.info('Running cmake: %s', cmake_cmd)
    subprocess.run(cmake_cmd, env=env)


def compile(env):
    compile_cmd = ['ninja', '-C', 'build']
    logging.info('Running ninja: %s', compile_cmd)
    subprocess.run(compile_cmd, env=env)


def install(env):
    install_cmd = ['ninja', '-C', 'build', 'install']
    logging.info('Running ninja install: %s', install_cmd)
    subprocess.run(install_cmd, env=env)


def cmake_test(env):
    workdir = os.path.join(os.getcwd(), 'build')
    ctest_cmd = [
        'ctest',
        '--label-exclude',
        '(standalone|sharded|single_node_replset|three_node_replset)'
    ]
    subprocess.run(ctest_cmd, cwd=workdir, env=env)


def parse_args(args, os_family):
    parser = argparse.ArgumentParser(
        description='Script for building genny',
        epilog='Unknown positional arguments will be forwarded verbatim to the cmake invocation'
    )

    # Python can't natively check the distros of our supported platforms.
    # See https://bugs.python.org/issue18872 for more info.
    parser.add_argument('-d', '--linux-distro',
                        choices=['ubuntu1804', 'arch', 'rhel7', 'amazon2', 'not-linux'],
                        help='specify the linux distro you\'re on; if your system isn\'t available,'
                             ' please contact us at #workload-generation')
    parser.add_argument('-v', '--verbose', action='store_true')

    subparsers = parser.add_subparsers(
        dest='subcommand',
        description='subcommands perform specific actions; make sure you run this script without any '
                    'subcommand first to initialize the environment')
    subparsers.add_parser(
        'cmake-test', help='run cmake unit tests that don\'t connect to a MongoDB cluster')
    subparsers.add_parser('compile', help='just run the compile step for genny')
    subparsers.add_parser('install', help='just run the install step for genny')

    known_args, unknown_args = parser.parse_known_args(args)

    if os_family == 'Linux' and not known_args.subcommand and not known_args.linux_distro:
        raise ValueError('--linux-distro must be specified on Linux')

    return known_args, unknown_args


def main():
    os_family = platform.system()
    args, cmake_args = parse_args(sys.argv[1:], os_family)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

    # This code is temporary. BUILD-7624 will consolidate the locations on mac and Linux.
    toolchain_parent = {
        'Linux': '/opt',
        'Darwin': '/data/mci'
    }

    # Map of platform.system() to vcpkg's OS names.
    triplet_os = {
        'Darwin': 'osx',
        'Linux': 'linux',
        'NT': 'windows'
    }[os_family]

    url = get_toolchain_url(os_family, args.linux_distro)
    toolchain_dir = fetch_and_install_toolchain(url, toolchain_parent[os_family])
    env = _create_compile_environment(toolchain_dir, triplet_os=triplet_os)

    if not args.subcommand:
        logging.info('No subcommand specified; running cmake, compile and install')
        cmake(toolchain_dir, cmdline_cmake_args=cmake_args, triplet_os=triplet_os, env=env)
        compile(env)
        install(env)
    else:
        # Always compile genny regardless of the subcommand.
        compile(env)

    if args.subcommand == 'install':
        install(env)
    if args.subcommand == 'cmake-test':
        cmake_test(env)


if __name__ == '__main__':
    main()
