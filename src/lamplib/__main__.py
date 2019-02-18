import logging
import os
import platform
import subprocess
import sys

import parser
import tasks
import tasks.run_tests

from context import Context
from parser import add_args_to_context

from tasks.toolchain import get_toolchain_url, fetch_and_install_toolchain


def main():
    os_family = platform.system()
    args, cmake_args = parser.parse_args(sys.argv[1:], os_family)

    # Execute the minimum amount of code possible to run self tests.
    if args.subcommand == 'self-test':
        res = subprocess.run(['python3', '-m', 'unittest'],
                             cwd=os.path.dirname(os.path.abspath(__file__)))
        res.check_returncode()
        sys.exit(0)

    add_args_to_context(args)

    # This code is temporary. BUILD-7624 will move things over to /opt/.
    toolchain_parent = {
        'Linux': '/data/mci',
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
    env = Context.get_compile_environment(toolchain_dir, triplet_os=triplet_os)

    if not args.subcommand:
        logging.info('No subcommand specified; running cmake, compile and install')
        tasks.cmake(toolchain_dir, cmdline_args=args, cmdline_cmake_args=cmake_args, triplet_os=triplet_os, env=env)
        tasks.compile_all(env, args)
        tasks.install(env, args)
    else:
        # Always compile genny regardless of the subcommand.
        tasks.compile_all(env, args)

    if args.subcommand == 'install':
        tasks.install(env, args)
    elif args.subcommand == 'cmake-test':
        tasks.run_tests.cmake_test(env, args)


if __name__ == '__main__':
    main()
