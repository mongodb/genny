import click
import platform
import structlog
import sys
import os
import loggers

SLOG = structlog.get_logger(__name__)


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
# TODO
#     if os_family == "Linux" and not known_args.subcommand and not known_args.linux_distro:
#         raise ValueError("--linux-distro must be specified on Linux")
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

    # TODO: barf if not set or not exists.
    ctx.obj["GENNY_REPO_ROOT"] = os.environ["GENNY_REPO_ROOT"]
    os.chdir(ctx.obj["GENNY_REPO_ROOT"])

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


@requires_build_system.command("install")
@click.pass_context
def self_test(ctx):
    from tasks import compile

    compile.install(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )


@requires_build_system.command("cmake-test")
@click.pass_context
def cmake_test(ctx):
    from tasks import run_tests

    run_tests.cmake_test(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["GENNY_REPO_ROOT"],
    )


@requires_build_system.command("benchmark-test")
@click.pass_context
def benchmark_test(ctx):
    from tasks import run_tests

    run_tests.benchmark_test(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["GENNY_REPO_ROOT"],
    )


@requires_build_system.command("workload")
@click.pass_context
@click.argument("genny_args", nargs=-1)
def workload(ctx, genny_args):
    from tasks import compile
    from tasks import genny_runner

    # Is there a better way to depend on `run-genny install`?
    compile.install(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )

    genny_runner.main_genny_runner(genny_args)


@requires_build_system.command("canaries")
@click.pass_context
def canaries(ctx):
    from tasks import compile
    from tasks import canaries_runner

    # Is there a better way to depend on `run-genny install`?
    compile.install(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )

    canaries_runner.main_canaries_runner()


@requires_build_system.command("resmoke-test")
@click.option("--suites", required=False, help='equivalent to resmoke.py\'s "--suites" option')
@click.option(
    "--create-new-actor-test-suite",
    is_flag=True,
    required=False,
    help='Run the "genny_create_new_actor" resmoke test suite, incompatible with the --suites options',
)
@click.option(
    "--mongo-dir",
    type=click.Path(),
    required=True,
    help="path to the mongo repo, which contains buildscripts/resmoke.py",
)
@click.pass_context
def resmoke_test(ctx, suites, create_new_actor_test_suite: bool, mongo_dir: str):
    from tasks import compile
    from tasks import run_tests

    # Is there a better way to depend on `run-genny install`?
    compile.install(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )

    run_tests.resmoke_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        suites=suites,
        is_cnats=create_new_actor_test_suite,
        mongo_dir=mongo_dir,
        env=os.environ.copy(),
    )


@requires_build_system.command("create-new-actor")
@click.argument("actor_name")
@click.pass_context
# TODO: this doesn't require the build-system (cmake) but shrug.
def create_new_actor(ctx, actor_name):
    import subprocess

    path = os.path.join(ctx.obj["GENNY_REPO_ROOT"], "src", "lamplib", "create-new-actor.sh")
    res = subprocess.run([path, actor_name], cwd=ctx.obj["GENNY_REPO_ROOT"])
    res.check_returncode()


# TODO: this doesn't require the build-system (cmake) but shrug.
@requires_build_system.command("self-test")
@click.pass_context
def self_test(ctx):
    from tasks import run_tests

    run_tests.run_self_test()


# TODO: this doesn't require the build-system (cmake) but shrug.
@requires_build_system.command("lint-yaml")
@click.pass_context
def lint_yaml(ctx):
    from tasks import yaml_linter

    yaml_linter.main()


# TODO: this doesn't require the build-system (cmake) but shrug.
@requires_build_system.command("auto-tasks")
@click.option(
    "--tasks", required=True, type=click.Choice(["all_tasks", "variant_tasks", "patch_tasks",]),
)
@click.pass_context
def auto_tasks(ctx, tasks):
    from tasks import auto_tasks

    auto_tasks.main(tasks)


# TODO: default subcommand
# loggers.info("No subcommand specified; running cmake, compile and install")


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    requires_build_system()
