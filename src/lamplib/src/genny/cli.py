import platform
import structlog
import sys
import os

from typing import List, Optional
import click
from click_option_group import optgroup, RequiredMutuallyExclusiveOptionGroup

SLOG = structlog.get_logger(__name__)


@click.group(name="Genny", context_settings=dict(help_option_names=["-h", "--help"]))
@click.option("-v", "--verbose", default=False, is_flag=True, help="Enable verbose output/logging.")
@click.pass_context
def cli(ctx: click.Context, verbose: bool) -> None:
    from genny import loggers

    # Ensure that ctx.obj exists and is a dict.
    ctx.ensure_object(dict)

    ctx.obj["VERBOSE"] = verbose
    loggers.setup_logging(verbose=verbose)

    found = os.environ.get("GENNY_REPO_ROOT", None)
    if not found or not os.path.exists(found):
        raise Exception(
            f"GENNY_REPO_ROOT env var {found} either not set or does not exist. "
            f"This is set when you run through the 'run-genny' wrapper."
        )
    ctx.obj["GENNY_REPO_ROOT"] = found

    ctx.obj["WORKSPACE_ROOT"] = os.getcwd()


@cli.command("install", help="Compile and install. This is required for most other commands.")
@click.option(
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
)
@click.option(
    "-i",
    "--ignore-toolchain-version",
    is_flag=True,
    help="Ignore the toolchain version. This is useful for testing toolchain changes.",
)
@click.option(
    "-b",
    "--build-system",
    type=click.Choice(["make", "ninja"]),
    default="ninja",
    help="Which build-system to use for compilation. May need to use make for IDEs.",
)
@click.option(
    "-s",
    "--sanitizer",
    type=click.Choice(["asan", "tsan", "ubsan"]),
    help=(
        "Compile with sanitizers enabled. "
        "This is only useful on the 'compile' task "
        "or when compiling for the first time."
    ),
)
@click.option(
    "-f",
    "--os-family",
    default=platform.system(),
    help=(
        "Override the value of Python's platform.system() "
        "when determining which version of the genny toolchain "
        "and curator to download. "
        "This is useful for testing toolchain changes."
    ),
)
@click.argument("cmake_args", nargs=-1)
@click.pass_context
def cmake_compile_install(
    ctx: click.Context,
    linux_distro: str,
    ignore_toolchain_version: bool,
    build_system: str,
    sanitizer: str,
    os_family: str,
    cmake_args: List[str],
):
    ctx.ensure_object(dict)

    from genny.tasks import compile
    from genny import curator

    curator.ensure_curator_installed(
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        os_family=os_family,
        linux_distro=linux_distro,
    )

    compile.compile_and_install(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        build_system=build_system,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
        sanitizer=sanitizer,
        cmake_args=cmake_args,
    )


@cli.command(
    "evaluate",
    help=(
        "Print the evaluated YAML workload file with minimal validation."
    ),
)
@click.argument("workload_path")
@click.pass_context
def create_new_actor(ctx: click.Context, workload_path: str):
    from genny.tasks import preprocess

    preprocess.evaluate(workload_path=workload_path)

@cli.command(
    name="clean", help="Resets output and venv directories to clean checkout state.",
)
@click.pass_context
def clean(ctx: click.Context) -> None:
    from genny.tasks import compile

    compile.clean(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"])


@cli.command(name="cmake-test", help="Run genny's C++ unit tests.")
@click.pass_context
def cmake_test(ctx: click.Context) -> None:
    from genny.tasks import run_tests

    run_tests.cmake_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], workspace_root=ctx.obj["WORKSPACE_ROOT"]
    )


@cli.command(
    name="benchmark-test",
    help="Run benchmark tests that assert genny's internals are sufficiently performant.",
)
@click.pass_context
def benchmark_test(ctx: click.Context) -> None:
    from genny.tasks import run_tests

    run_tests.benchmark_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], workspace_root=ctx.obj["WORKSPACE_ROOT"]
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
@click.argument("genny_args", nargs=-1)
@click.pass_context
def workload(ctx: click.Context, genny_args: List[str]):
    from genny.tasks import genny_runner

    ctx.ensure_object(dict)
    ctx.obj["GENNY_ARGS"] = genny_args

    genny_runner.main_genny_runner(
        genny_args=ctx.obj["GENNY_ARGS"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
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
@click.pass_context
def dry_run_workloads(ctx: click.Context):
    from genny.tasks import dry_run

    dry_run.dry_run_workloads(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], workspace_root=ctx.obj["WORKSPACE_ROOT"]
    )


@cli.command(
    name="canaries",
    help=(
        "Run genny's canaries that exercise workload primitives but don't interact with "
        "noisy systems like storage. We expect these canaries to have very little "
        "'noise' over time, such that if the canaries' performance changes significantly "
        "run-over-run it is indicative of a change in the underlying system."
    ),
)
# f"{prefix} ./src/genny/run-genny canaries nop",
# f"{prefix} ./src/genny/run-genny canaries ping -- --mongo-uri '{db_url}'"
@click.argument("canary_args", nargs=-1)
@click.pass_context
def canaries(ctx: click.Context, canary_args: List[str]):
    from genny.tasks import canaries_runner

    canaries_runner.main_canaries_runner(
        canary_args=canary_args,
        cleanup_metrics=True,
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
    )


# noinspection PyArgumentEqualDefault
@cli.command(
    name="resmoke-test",
    help="Runs genny's C++-based integration tests under resmoke (lives in the mongo repo)",
)
@click.option(
    "--mongo-dir",
    type=click.Path(
        exists=False, file_okay=False, dir_okay=True, writable=True, resolve_path=True,
    ),
    required=False,
    default=None,
    help="Path to the mongo repo, which contains buildscripts/resmoke.py",
)
@click.option(
    "--mongodb-archive-url",
    type=str,
    required=False,
    default=None,
    help=(
        "The URL of a 'Binaries' artifact containing mongod, mongos, mongo. "
        "Not needed if you have built/installed these tools in hte --mongo-dir."
    ),
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
def resmoke_test(
    ctx: click.Context,
    suites: str,
    create_new_actor_test_suite: bool,
    mongo_dir: Optional[str],
    mongodb_archive_url: Optional[str],
):
    from genny.tasks import run_tests

    run_tests.resmoke_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        suites=suites,
        is_cnats=create_new_actor_test_suite,
        mongo_dir=mongo_dir,
        env=os.environ.copy(),
        mongodb_archive_url=mongodb_archive_url,
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
def create_new_actor(ctx: click.Context, actor_name: str):
    from genny.tasks import create_new_actor

    create_new_actor.run_create_new_actor(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], actor_name=actor_name,
    )


@cli.command(
    name="lint-python",
    help="Run the 'black' python format checker to ensure genny's internal python is 💅.",
)
@click.option(
    "--fix", default=False, is_flag=True, help="Fix formatting in-place rather than erroring."
)
@click.pass_context
def lint_python(ctx: click.Context, fix: bool):
    from genny.tasks import lint_python

    lint_python.lint_python(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], fix=fix)


@cli.command(name="self-test", help="Run the pytest tests of genny's internal python.")
@click.pass_context
def self_test(ctx: click.Context):
    from genny.tasks import pytest

    pytest.run_self_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"], workspace_root=ctx.obj["WORKSPACE_ROOT"]
    )


@cli.command(name="lint-yaml", help="Run pylint on all workload and phase yamls")
@click.pass_context
def lint_yaml(ctx: click.Context):
    from genny.tasks import yaml_linter

    yaml_linter.main(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"])


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
def auto_tasks(ctx: click.Context, tasks: str):
    from genny.tasks import auto_tasks

    auto_tasks.main(
        mode_name=tasks,
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
    )


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    cli()
