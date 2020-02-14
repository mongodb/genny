import argparse
import logging

from context import Context


def parse_args(args, os_family):
    parser = argparse.ArgumentParser(
        description='Script for building genny',
        epilog='Unknown positional arguments will be forwarded verbatim to the cmake'
               ' invocation where relevant'
    )

    # Python can't natively check the distros of our supported platforms.
    # See https://bugs.python.org/issue18872 for more info.
    parser.add_argument('-d', '--linux-distro',
                        choices=['ubuntu1804', 'archlinux', 'rhel70', 'rhel62', 'amazon2', 'not-linux'],
                        help='specify the linux distro you\'re on; if your system isn\'t available,'
                             ' please contact us at #workload-generation')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-g', '--run-global', action='store_true',
                        help='allow installation outside of a virtualenv')
    parser.add_argument('-i', '--ignore-toolchain-version', action='store_true',
                        help='ignore the toolchain version, useful for testing toolchain changes')
    parser.add_argument('-b', '--build-system',
                        choices=['make', 'ninja'], default='ninja',
                        help='Which build-system to use for compilation. May need to use make for '
                             'IDEs.')
    parser.add_argument('-s', '--sanitizer', choices=['asan', 'tsan', 'ubsan'])

    subparsers = parser.add_subparsers(
        dest='subcommand',
        description='subcommands perform specific actions; make sure you run this script without '
                    'any subcommand first to initialize the environment')
    subparsers.add_parser(
        'cmake-test', help='run cmake unit tests that don\'t connect to a MongoDB cluster')
    subparsers.add_parser('benchmark-test', help='run benchmark unit tests')

    resmoke_test_parser = subparsers.add_parser(
        'resmoke-test', help='run cmake unit tests that connect to a MongoDB cluster')
    group = resmoke_test_parser.add_mutually_exclusive_group()
    group.add_argument('--suites', dest='resmoke_suites',
                       help='equivalent to resmoke.py\'s "--suites" option')
    group.add_argument('--create-new-actor-test-suite', action='store_true', dest='resmoke_cnats',
                       help='Run the "genny_create_new_actor" resmoke test suite,'
                            ' incompatible with the --suites options')
    resmoke_test_parser.add_argument('--mongo-dir', dest='resmoke_mongo_dir',
                       help='path to the mongo repo, which contains buildscripts/resmoke.py')

    subparsers.add_parser('install', help='just run the install step for genny')
    subparsers.add_parser('clean', help='cleanup existing build')
    subparsers.add_parser('self-test', help='run lamplib unittests')

    known_args, unknown_args = parser.parse_known_args(args)

    if os_family == 'Linux' and not known_args.subcommand and not known_args.linux_distro:
        raise ValueError('--linux-distro must be specified on Linux')

    return known_args, unknown_args


def add_args_to_context(args):
    """
    Add command line arguments to the global context object to be used later on.

    Consider putting command line arguments onto the context if it is used by more
    than one caller.

    :param args:
    :return:
    """
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
    Context.IGNORE_TOOLCHAIN_VERSION = args.ignore_toolchain_version
    Context.BUILD_SYSTEM = args.build_system
    Context.SANITIZER = args.sanitizer
