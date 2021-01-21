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
            "Specify the linux distro you're on; if your system isn't available,"
            " please contact us at #workload-generation. The not-linux value is useful on macOS."
        ),
    ),
    click.option(
        "-i",
        "--ignore-toolchain-version",
        is_flag=True,
        help="Ignore the toolchain version. " "This is useful for testing toolchain changes.",
    ),
    click.option(
        "-b",
        "--build-system",
        type=click.Choice(["make", "ninja"]),
        default="ninja",
        help="Which build-system to use for compilation. May need to use make for IDEs.",
    ),
    click.option(
        "-s",
        "--sanitizer",
        type=click.Choice(["asan", "tsan", "ubsan"]),
        help=(
            "Compile with sanitizers enabled. "
            "This is only useful on the 'compile' task "
            "or when compiling for the first time."
        ),
    ),
    click.option(
        "-f",
        "--os-family",
        default=platform.system(),
        help=(
            "Override the value of Python's platform.system() "
            "when determining which version of the genny toolchain "
            "and curator to download. "
            "This is useful for testing toolchain changes."
        ),
    ),
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

    curator.ensure_curator_installed(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        os_family=ctx.obj["OS_FAMILY"],
        distro=ctx.obj["LINUX_DISTRO"],
    )


@click.group()
def cli():
    pass


@cli.command(
    name="compile",
    help=(
        "Run cmake and compile. "
        "This does not populate or update the 'dist' directory "
        "where genny's output is installed by default."
        "To do that, run install."
    ),
)
@build_system_options
@click.pass_context
def compile_op(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=False)


@cli.command(
    name="clean", help="Resets output and venv directories to clean checkout state.",
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


@cli.command(name="install", help="Install build output to the output location (dist by default).")
@build_system_options
@click.pass_context
def install(ctx, **kwargs) -> None:
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)


@cli.command(name="cmake-test", help="Run genny's C++ unit tests.")
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


@cli.command(
    name="benchmark-test",
    help="Run benchmark tests that assert genny's internals are sufficiently performant.",
)
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


@cli.command(
    name="workload",
    help=(
        "Actually calls the underlying genny binary called genny_core "
        "which is installed by default to 'dist'. "
        "Pass '--' to pass additional args to genny_core, e.g."
        "run-genny workload -- run -w path_to_yaml -u mongodb://localhost:271017"
    ),
)
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
        cleanup_metrics=True,
    )


@cli.command(
    name="dry-run-workloads",
    help=(
        "Iterates over all yaml files src/workloads and asserts that "
        "the workload context be constructed (all Actors can be constructed) "
        "but does NOT actually run the workload. This is useful when your Actor's"
        "constructor validates configuration at constructor time."
    ),
)
@build_system_options
@click.pass_context
def dry_run_workloads(ctx, **kwargs):
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import dry_run

    dry_run.dry_run_workloads(ctx.obj["GENNY_REPO_ROOT"], ctx.obj["OS_FAMILY"])


@cli.command(
    name="canaries",
    help=(
        "Run genny's canaries that exercise workload primitives but don't interact with "
        "external systems like databases. We expect these canaries to have very little "
        "'noise' over time such that if the canaries' performance changes significantly "
        "run-over-run it is indicative of a change in the underlying system."
    ),
)
@build_system_options
@click.pass_context
def canaries(ctx, **kwargs):
    setup(ctx, **kwargs)
    cmake_compile_install(ctx, perform_install=True)

    from tasks import canaries_runner

    canaries_runner.main_canaries_runner(cleanup_metrics=True)


@cli.command(
    name="resmoke-test",
    help="Runs genny's C++-based integration tests under resmoke (lives in the mongo repo)",
)
@build_system_options
@click.option(
    "--mongo-dir",
    type=click.Path(),
    required=False,
    default=None,
    help="Path to the mongo repo, which contains buildscripts/resmoke.py",
)
@optgroup.group("Type of resmoke task to run", cls=RequiredMutuallyExclusiveOptionGroup)
@optgroup.option("--suites", help='equivalent to resmoke.py\'s "--suites" option')
@optgroup.option(
    "--create-new-actor-test-suite",
    is_flag=True,
    help=(
        'Run the "genny_create_new_actor" resmoke test suite.'
        "This is incompatible with the --suites options"
    ),
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


@cli.command(
    "create-new-actor",
    help=(
        "Generate the skeleton code for a new C++-based Actor including "
        "headers, implementation, and a basic unit/integration test."
    ),
)
@click.argument("actor_name")
@click.pass_context
def create_new_actor(ctx, actor_name):
    import subprocess

    path = os.path.join(
        ctx.obj["GENNY_REPO_ROOT"], "src", "lamplib", "tasks", "create-new-actor.sh"
    )
    res = subprocess.run([path, actor_name], cwd=ctx.obj["GENNY_REPO_ROOT"])
    res.check_returncode()


@cli.command(
    name="lint-python",
    help="Run the 'black' python format checker to ensure genny's internal python is 💅.",
)
@click.pass_context
def lint_python(ctx):
    from tasks import lint_python

    # TODO: add --fix option
    genny_repo_root = os.environ.get("GENNY_REPO_ROOT")
    lint_python.lint_python(genny_repo_root)


@cli.command(name="self-test", help="Run the pytest tests of genny's internal python.")
@click.pass_context
def self_test(ctx):
    from tasks import run_tests

    run_tests.run_self_test()


@cli.command(name="lint-yaml", help="Run pylint on all workload and phase yamls")
@click.pass_context
def lint_yaml(ctx):
    from tasks import yaml_linter

    yaml_linter.main()


@cli.command(
    name="auto-tasks",
    help=(
        "Determine which Genny workloads to run given the current build variant etc. "
        "as determined by the ./expansions.yml file. "
        "This is used by evergreen and allows new genny workloads to be created "
        "without having to modify any repos outside of genny itself."
    ),
)
@click.option(
    "--tasks", required=True, type=click.Choice(["all_tasks", "variant_tasks", "patch_tasks"]),
)
@click.pass_context
def auto_tasks(ctx, tasks):
    from tasks import auto_tasks

    auto_tasks.main(tasks)


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    cli()
