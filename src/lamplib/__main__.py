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


def run_self_test():
    res = subprocess.run(['python3', '-m', 'unittest'],
                         cwd=os.path.dirname(os.path.abspath(__file__)))
    res.check_returncode()
    sys.exit(0)


def main():
    # Initialize the global context.
    os_family = platform.system()
    Context.set_triplet_os(os_family)
    args, cmake_args = parser.parse_args(sys.argv[1:], os_family)
    add_args_to_context(args)
    # Pass around Context instead of using the global one to facilitate testing.
    context = Context

    # Execute the minimum amount of code possible to run self tests to minimize
    # untestable code (i.e. code that runs the self-test).
    if args.subcommand == 'self-test':
        run_self_test()

    url = get_toolchain_url(os_family, args.linux_distro)
    toolchain_dir = fetch_and_install_toolchain(url, Context.TOOLCHAIN_ROOT)
    compile_env = context.get_compile_environment(toolchain_dir)

    if not args.subcommand:
        logging.info('No subcommand specified; running cmake, compile and install')
        tasks.cmake(context, toolchain_dir=toolchain_dir,
                    env=compile_env, cmdline_cmake_args=cmake_args)
        tasks.compile_all(context, compile_env)
        tasks.install(context, compile_env)
    elif args.subcommand == 'clean':
        tasks.clean(context, compile_env)
    else:
        tasks.compile_all(context, compile_env)

        if args.subcommand == 'install':
            tasks.install(context, compile_env)
        elif args.subcommand == 'cmake-test':
            tasks.run_tests.cmake_test(compile_env)
        elif args.subcommand == 'resmoke-test':
            tasks.run_tests.resmoke_test(compile_env, suites=args.resmoke_suites,
                                         mongo_dir=args.resmoke_mongo_dir, is_cnats=args.resmoke_cnats)
        else:
            raise ValueError('Unknown subcommand: ', args.subcommand)


if __name__ == '__main__':
    main()
