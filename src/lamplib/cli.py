from typing import List

import click
from click_option_group import optgroup, RequiredMutuallyExclusiveOptionGroup
import platform
import structlog
import sys
import os

import loggers
import curator

SLOG = structlog.get_logger(__name__)

# Heavy inspiration from here: https://github.com/pallets/click/issues/108

_build_system_options = [
    click.option("-v", "--verbose", default=False, is_flag=True, help="Enable debug logging."),
    # Python can't natively check the distros of our supported platforms.
    # See https://bugs.python.org/issue18872 for more info.
    click.option(
        "-d",
        "--linux-distro",
        required=False,
        default="not-linux",
        type=click.Choice(
            ["ubuntu1804", "archlinux", "rhel8", "rhel70", "rhel62", "amazon2", "not-linux"]
        ),
        help=(
            "specify the linux distro you're on; if your system isn't available,"
            " please contact us at #workload-generation"
        ),
    ),
    # TODO
    #     if os_family == "Linux" and not known_args.subcommand and not known_args.linux_distro:
    #         raise ValueError("--linux-distro must be specified on Linux")
    click.option(
        "-i",
        "--ignore-toolchain-version",
        is_flag=True,
        help="ignore the toolchain version, useful for testing toolchain changes",
    ),
    click.option(
        "-b",
        "--build-system",
        type=click.Choice(["make", "ninja"]),
        default="ninja",
        help="Which build-system to use for compilation. May need to use make for IDEs.",
    ),
    click.option("-s", "--sanitizer", type=click.Choice(["asan", "tsan", "ubsan"]),),
    click.option("-f", "--os-family", default=platform.system(),),
    click.argument("cmake_args", nargs=-1),
]


def build_system_options(func):
    for option in reversed(_build_system_options):
        func = option(func)
    return func


# If ctx.obj interactions become any more complicated, please refactor to a Context objects
# that can be constructed from kwargs and can handle its own saving/cleaning, etc.


def setup(
    ctx,
    verbose: bool,
    linux_distro: str,
    ignore_toolchain_version: bool,
    build_system: str,
    sanitizer: str,
    os_family: str,
    cmake_args: List[str],
):
    ctx.ensure_object(dict)

    ctx.obj["VERBOSE"] = verbose
    ctx.obj["LINUX_DISTRO"] = linux_distro
    ctx.obj["IGNORE_TOOLCHAIN_VERSION"] = ignore_toolchain_version
    ctx.obj["BUILD_SYSTEM"] = build_system
    ctx.obj["SANITIZER"] = sanitizer
    ctx.obj["OS_FAMILY"] = os_family
    ctx.obj["CMAKE_ARGS"] = cmake_args

    loggers.setup_logging(verbose=ctx.obj["VERBOSE"])

    # TODO: barf if not set or not exists.
    ctx.obj["GENNY_REPO_ROOT"] = os.environ["GENNY_REPO_ROOT"]
    os.chdir(ctx.obj["GENNY_REPO_ROOT"])


def cmake_compile_install(ctx, perform_install: bool):
    from tasks import compile

    compile.cmake(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["SANITIZER"],
        ctx.obj["CMAKE_ARGS"],
    )

    compile.compile_all(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )
    if perform_install:
        compile.install(
            ctx.obj["BUILD_SYSTEM"],
            ctx.obj["OS_FAMILY"],
            ctx.obj["LINUX_DISTRO"],
            ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        )

    curator.ensure_curator(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        os_family=ctx.obj["OS_FAMILY"],
        distro=ctx.obj["LINUX_DISTRO"],
    )


@click.group()
def cli():
    pass


@cli.command(
    name="compile", help="Compile",
)
@build_system_options
@click.pass_context
def compile(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=False)


@cli.command(
    name="clean", help="Clean",
)
@build_system_options
@click.pass_context
def clean(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    from tasks import compile

    compile.clean(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
    )


@cli.command(
    name="install", help="Install",
)
@build_system_options
@click.pass_context
def install(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)


@cli.command(name="cmake-test",)
@build_system_options
@click.pass_context
def cmake_test(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import run_tests

    run_tests.cmake_test(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["GENNY_REPO_ROOT"],
    )


@cli.command(name="benchmark-test")
@build_system_options
@click.pass_context
def benchmark_test(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import run_tests

    run_tests.benchmark_test(
        ctx.obj["BUILD_SYSTEM"],
        ctx.obj["OS_FAMILY"],
        ctx.obj["LINUX_DISTRO"],
        ctx.obj["IGNORE_TOOLCHAIN_VERSION"],
        ctx.obj["GENNY_REPO_ROOT"],
    )


@cli.command(name="workload")
@build_system_options
@click.pass_context
def workload(ctx, **kwargs):
    # TODO: can't call cmake with same args here, since it ends up lookign like:
    # cmake -B build
    #       -G Ninja
    #       -DCMAKE_PREFIX_PATH=/data/mci/gennytoolchain/installed/x64-linux-shared
    #       -DCMAKE_TOOLCHAIN_FILE=/data/mci/gennytoolchain/scripts/buildsystems/vcpkg.cmake
    #       -DVCPKG_TARGET_TRIPLET=x64-linux-static
    #       -u mongodb://username:password@10.2.0.200:27017/admin?ssl=false
    #     ./etc/genny/workloads/docs/HelloWorld.yml
    # which ends up with a (benign but confusing) error:
    #    CMake Error: The source directory "/media/ephemeral0/src/genny/etc/genny/workloads/docs/HelloWorld.yml" does not exist.
    #
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import genny_runner

    # TODO: cmake_args needs to be renamed.
    genny_runner.main_genny_runner(
        genny_args=ctx.obj["CMAKE_ARGS"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        cleanup_metrics=False,
    )


@cli.command(name="dry-run-workloads")
@build_system_options
@click.pass_context
def dry_run_workloads(ctx, **kwargs):
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import dry_run

    dry_run.dry_run_workloads(ctx.obj["GENNY_REPO_ROOT"], ctx.obj["OS_FAMILY"])


@cli.command(name="canaries")
@build_system_options
@click.pass_context
def canaries(ctx, **kwargs):
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import canaries_runner

    canaries_runner.main_canaries_runner()


@cli.command("resmoke-test")
@build_system_options
@click.option(
    "--mongo-dir",
    type=click.Path(),
    required=True,
    help="path to the mongo repo, which contains buildscripts/resmoke.py",
)
@optgroup.group("Type of resmoke task to run", cls=RequiredMutuallyExclusiveOptionGroup)
@optgroup.option("--suites", help='equivalent to resmoke.py\'s "--suites" option')
@optgroup.option(
    "--create-new-actor-test-suite",
    is_flag=True,
    help='Run the "genny_create_new_actor" resmoke test suite, incompatible with the --suites options',
)
@click.pass_context
def resmoke_test(ctx, suites, create_new_actor_test_suite: bool, mongo_dir: str, **kwargs):
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import run_tests

    run_tests.resmoke_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        suites=suites,
        is_cnats=create_new_actor_test_suite,
        mongo_dir=mongo_dir,
        env=os.environ.copy(),
    )


@cli.command("create-new-actor")
@click.argument("actor_name")
@click.pass_context
def create_new_actor(ctx, actor_name):
    import subprocess

    path = os.path.join(
        ctx.obj["GENNY_REPO_ROOT"], "src", "lamplib", "tasks", "create-new-actor.sh"
    )
    res = subprocess.run([path, actor_name], cwd=ctx.obj["GENNY_REPO_ROOT"])
    res.check_returncode()


@cli.command("lint-python")
@click.pass_context
def lint_python(ctx):
    from tasks import lint_python

    genny_repo_root = os.environ.get("GENNY_REPO_ROOT")
    lint_python.lint_python(genny_repo_root)


@cli.command("self-test")
@click.pass_context
def self_test(ctx):
    from tasks import run_tests

    run_tests.run_self_test()


@cli.command("lint-yaml")
@click.pass_context
def lint_yaml(ctx):
    from tasks import yaml_linter

    yaml_linter.main()


@cli.command("auto-tasks")
@click.option(
    "--tasks", required=True, type=click.Choice(["all_tasks", "variant_tasks", "patch_tasks",]),
)
@click.pass_context
def auto_tasks(ctx, tasks):
    from tasks import auto_tasks

    auto_tasks.main(tasks)


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    cli()
