import click
import os
import platform
import subprocess
import structlog
import sys
import loggers

SLOG = structlog.get_logger(__name__)


def check_venv(args):
    if "VIRTUAL_ENV" not in os.environ and not args.run_global:
        SLOG.error("Tried to execute without active virtualenv.")
        sys.exit(1)


def run_self_test():
    cwd = os.path.dirname(os.path.abspath(__file__))
    SLOG.info("Running self-test", cwd=cwd)
    import pytest
    pytest.main([])
    SLOG.info("Self-test finished.")
    sys.exit(0)


def python_version_string():
    return ".".join(map(str, sys.version_info))[0:5]


def validate_environment():
    # Check Python version
    if not sys.version_info >= (3, 7):
        raise OSError(
            "Detected Python version {version} less than 3.7. Please delete "
            "the virtualenv and run lamp again.".format(version=python_version_string())
        )

    # Check the macOS version. Non-mac platforms return a tuple of empty strings
    # for platform.mac_ver().
    if platform.mac_ver()[0] == "10":
        release_triplet = platform.mac_ver()[0].split(".")
        if int(release_triplet[1]) < 14:
            # You could technically compile clang or gcc yourself on an older version
            # of macOS, but it's untested so we might as well just enforce
            # a blanket minimum macOS version for simplicity.
            SLOG.error("Genny requires macOS 10.14 Mojave or newer")
            sys.exit(1)
    return


CONTEXT_SETTINGS = dict(help_option_names=["-h", "--help"])


# @click.group(name="DSI", context_settings=CONTEXT_SETTINGS)
#
# @click.pass_context
# def cli(ctx: click.Context, debug: bool) -> None:
#     # Ensure that ctx.obj exists and is a dict.
#     ctx.ensure_object(dict)
#
#     ctx.obj["DEBUG"] = debug
#

"""
#     parser.add_argument(
#         "-b",
#         "--build-system",
#         choices=["make", "ninja"],
#         default="ninja",
#         help="Which build-system to use for compilation. May need to use make for " "IDEs.",
#     )
"""


@click.group()
@click.option("-v", "--verbose", default=False, is_flag=True, help="Enable debug logging.")
# Python can't natively check the distros of our supported platforms.
# See https://bugs.python.org/issue18872 for more info.
@click.option(
    "-d",
    "--linux-distro",
    required=False,
    default="not-linux",
    type=click.Choice(
        ["ubuntu1804", "archlinux", "rhel8", "rhel70", "rhel62", "amazon2", "not-linux"]
    ),
    help="specify the linux distro you're on; if your system isn't available,"
    " please contact us at #workload-generation",
)
@click.option(
    "-i",
    "--ignore-toolchain-version",
    help="ignore the toolchain version, useful for testing toolchain changes",
)
@click.option(
    "-b",
    "--build-system",
    type=click.Choice(["make", "ninja"]),
    default="ninja",
    help="Which build-system to use for compilation. May need to use make for IDEs.",
)
@click.option(
    "-s", "--sanitizer", type=click.Choice(["asan", "tsan", "ubsan"]),
)
@click.option(
    "-f", "--os-family", default=platform.system(),
)
@click.pass_context
def requires_build_system(
    ctx,
    verbose: bool,
    linux_distro: str,
    ignore_toolchain_version: bool,
    build_system: str,
    sanitizer: str,
    os_family: str,
):
    ctx.ensure_object(dict)
    ctx.obj["VERBOSE"] = verbose
    ctx.obj["LINUX_DISTRO"] = linux_distro
    ctx.obj["IGNORE_TOOLCHAIN_VERSION"] = ignore_toolchain_version
    ctx.obj["BUILD_SYSTEM"] = build_system
    ctx.obj["SANITIZER"] = sanitizer
    ctx.obj["OS_FAMILY"] = os_family

    loggers.setup_logging(verbose=ctx.obj["VERBOSE"])

    from tasks import compile

    compile.cmake(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["SANITIZER"],
    )


@requires_build_system.command(
    name="compile", help="Compile",
)
@click.pass_context
def compile(ctx) -> None:
    from tasks import compile

    compile.compile_all(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )


@requires_build_system.command("clean")
@click.pass_context
def clean(ctx) -> None:
    from tasks import compile

    compile.clean(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )


# TODO: this doesn't require the build-system (cmake) but shrug.
@requires_build_system.command("self-test")
@click.pass_context
def self_test(ctx):
    run_self_test()


# TODO: common stuff
#
#     check_venv(args)
#
#     # Initialize the global context.
#     os_family = platform.system()
#     Context.set_triplet_os(os_family)
#
#     # TODO: barf if not set or not exists.
#     os.chdir(os.environ["GENNY_REPO_ROOT"])
#
#
#     curator_downloader = CuratorDownloader(os_family, args.linux_distro)
#     if not curator_downloader.fetch_and_install():
#         sys.exit(1)
#

# TODO: default subcommand
#
#     if not args.subcommand:
#         loggers.info("No subcommand specified; running cmake, compile and install")
#         tasks.cmake(
#             context, toolchain_dir=toolchain_dir, env=compile_env, cmdline_cmake_args=cmake_args
#         )
#         tasks.compile_all(context, compile_env)
#         tasks.install(context, compile_env)
#     elif args.subcommand == "clean":
#         tasks.clean(context, compile_env)
#     else:
#         tasks.compile_all(context, compile_env)
#         if args.subcommand == "install":
#             tasks.install(context, compile_env)
#         elif args.subcommand == "cmake-test":
#             tasks.run_tests.cmake_test(compile_env)
#         elif args.subcommand == "benchmark-test":
#             tasks.run_tests.benchmark_test(compile_env)
#         elif args.subcommand == "resmoke-test":
#             tasks.run_tests.resmoke_test(
#                 compile_env,
#                 suites=args.resmoke_suites,
#                 mongo_dir=args.resmoke_mongo_dir,
#                 is_cnats=args.resmoke_cnats,
#             )
#         else:
#             raise ValueError("Unknown subcommand: ", args.subcommand)
#


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    requires_build_system()


#
#
#     subparsers = parser.add_subparsers(
#         dest="subcommand",
#         description="subcommands perform specific actions; make sure you run this script without "
#         "any subcommand first to initialize the environment",
#     )
#     subparsers.add_parser(
#         "cmake-test", help="run cmake unit tests that don't connect to a MongoDB cluster"
#     )
#     subparsers.add_parser("benchmark-test", help="run benchmark unit tests")
#
#     resmoke_test_parser = subparsers.add_parser(
#         "resmoke-test", help="run cmake unit tests that connect to a MongoDB cluster"
#     )
#     group = resmoke_test_parser.add_mutually_exclusive_group()
#     group.add_argument(
#         "--suites", dest="resmoke_suites", help='equivalent to resmoke.py\'s "--suites" option'
#     )
#     group.add_argument(
#         "--create-new-actor-test-suite",
#         action="store_true",
#         dest="resmoke_cnats",
#         help='Run the "genny_create_new_actor" resmoke test suite,'
#         " incompatible with the --suites options",
#     )
#     resmoke_test_parser.add_argument(
#         "--mongo-dir",
#         dest="resmoke_mongo_dir",
#         help="path to the mongo repo, which contains buildscripts/resmoke.py",
#     )
#
#     subparsers.add_parser("install", help="just run the install step for genny")
#     subparsers.add_parser("clean", help="cleanup existing build")
#     subparsers.add_parser("self-test", help="run lamplib unittests")
#
#     known_args, unknown_args = parser.parse_known_args(args)
#
#     if os_family == "Linux" and not known_args.subcommand and not known_args.linux_distro:
#         raise ValueError("--linux-distro must be specified on Linux")
#
#     return known_args, unknown_args
#
#
# def add_args_to_context(args):
#     """
#     Add command line arguments to the global context object to be used later on.
#
#     Consider putting command line arguments onto the context if it is used by more
#     than one caller.
#
#     :param args:
#     :return:
#     """
#     loggers.basicConfig(level=loggers.DEBUG if args.verbose else loggers.INFO)
#     Context.IGNORE_TOOLCHAIN_VERSION = args.ignore_toolchain_version
#     Context.BUILD_SYSTEM = args.build_system
#     Context.SANITIZER = args.sanitizer
