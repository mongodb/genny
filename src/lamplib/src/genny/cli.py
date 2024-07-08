import platform
import structlog
import sys
import os

from typing import List, Optional, Tuple
import click
from click_option_group import optgroup, RequiredMutuallyExclusiveOptionGroup

SLOG = structlog.get_logger(__name__)


@click.group(name="Genny", context_settings=dict(help_option_names=["-h", "--help"]))
@click.option("-v", "--verbose", default=False, is_flag=True, help="Enable verbose output/logging.")
@click.pass_context
def cli(ctx: click.Context, verbose: bool) -> None:
    """
    🧞 Genny Version 0.0.1 💝🐹🌇⛔

    To run workloads, try ./run-genny workload -h

    For more information, see the docs: https://github.com/mongodb/genny
    """
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
    default=None,
    type=click.Choice(
        [
            "ubuntu1804",
            "ubuntu2004",
            "ubuntu2004_arm64",
            "ubuntu2204",
            "ubuntu2204_arm64",
            "rhel8",
            "rhel70",
            "amazon2",
            "amazon2_arm64",
            "not-linux",
        ]
    ),
    help=(
        "Specify the linux distro you're on; if your system isn't available,"
        " please contact us at #ask-devprod-performance. Specify not-linux for macOS."
        " If no value is specified, it will be autodetected."
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

    if linux_distro is None:
        linux_distro = compile.detect_distro()

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
    help=("Evaluate the YAML workload file with minimal validation."),
)
@click.argument("workload_path")
@click.option(
    "-u",
    "--mongo-uri",
    required=False,
    default="mongodb://localhost:27017",
    help=("Set a default mongo uri used by connection pools that don't have one configured."),
)
@click.option(
    "--mongostream-uri",
    required=False,
    default=None,
    help=(
        "Set a mongostream uri used by the stream connection pool, if this is not "
        "set then this will default to the --mongo-uri."
    ),
)
@click.option(
    "-o",
    "--output",
    required=False,
    default=None,
    help=(
        "Filepath where the output of the evaluation will be written. Will write to stdout by default."
    ),
)
@click.option(
    "-v",
    "--override",
    required=False,
    default=None,
    help=("Filepath of an override file. Use this to override workload configs."),
)
@click.option(
    "-s",
    "--smoke",
    is_flag=True,
    help=(
        "Convert a workload YAML into a version for smoke test where every phase"
        " of every actor runs with Repeat: 1."
    ),
)
@click.pass_context
def evaluate(
    ctx: click.Context,
    workload_path: str,
    mongo_uri: str,
    output: str,
    override: str,
    smoke: bool,
    mongostream_uri: Optional[str],
):
    from genny.tasks import preprocess

    preprocess.evaluate(
        workload_path=workload_path,
        default_uri=mongo_uri,
        smoke=smoke,
        override_file_path=override,
        output=output,
        mongostream_uri=mongostream_uri,
    )


@cli.command(
    "export",
    help=("Export the given FTDC file to CSV."),
)
@click.argument("ftdc_path")
@click.option(
    "-o",
    "--output",
    required=False,
    default=None,
    help=("Filepath where the output CSV will be written. Will write to stdout by default."),
)
@click.pass_context
def export_ftdc_to_csv(ctx: click.Context, ftdc_path: str, output):
    from genny.curator import export

    export(
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        input_path=ftdc_path,
        output_path=output,
    )


@cli.command(
    "translate",
    help=("Translate the given genny workload directory to t2 ftdc."),
)
@click.argument("ftdc_path")
@click.option(
    "-o",
    "--output",
    required=False,
    default=None,
    help=("Filepath where the output ftdc will be written. Will write to stdout by default."),
)
@click.pass_context
def translate(ctx: click.Context, ftdc_path: str, output):
    from genny.curator import translate

    translate(
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        input_path=ftdc_path,
        output_path=output,
    )


@cli.command(
    name="clean",
    help="Resets output and venv directories to clean checkout state.",
)
@click.pass_context
def clean(ctx: click.Context) -> None:
    from genny.tasks import compile

    compile.clean(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"])


@cli.command(name="cmake-test", help="Run genny's C++ unit tests.")
@click.option(
    "-g",
    "--regex",
    required=False,
    default=None,
    help=("Regex to match against tests."),
)
@click.option(
    "-E",
    "--regex-exclude",
    required=False,
    default=None,
    help=("Exclude tests names of which match this regex."),
)
@click.option(
    "-r",
    "--repeat-until-fail",
    required=False,
    default=1,
    help=(
        "Repeat this many times, unless a test fails. Default is 1. This is useful for flaky tests."
    ),
)
@click.pass_context
def cmake_test(ctx: click.Context, regex: str, regex_exclude: str, repeat_until_fail: int) -> None:
    from genny.tasks import run_tests

    run_tests.cmake_test(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        regex=regex,
        regex_exclude=regex_exclude,
        repeat_until_fail=repeat_until_fail,
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
    help=("Actually run a workload and place results in `build/WorkloadOutput`."),
)
@click.argument("workload_yaml", nargs=-1)
@click.option(
    "-u",
    "--mongo-uri",
    required=False,
    default="mongodb://localhost:27017",
    help=("Set a default mongo uri used by connection pools that don't have one configured."),
)
@click.option(
    "--mongostream-uri",
    required=False,
    default=None,
    help=(
        "Set a mongostream uri used by the stream connection pool, if this is not "
        "set then this will default to the --mongo-uri."
    ),
)
@click.option(
    "-v",
    "--verbosity",
    required=False,
    default="info",
    help=("Log severity for boost logging. Valid values are trace/debug/info/warning/error/fatal."),
)
@click.option(
    "-o",
    "--override",
    required=False,
    default=None,
    help=("Specify an override file to be merged with the specified workload yaml."),
)
@click.option(
    "-d",
    "--dry-run",
    required=False,
    default=False,
    is_flag=True,
    help=("Exit before the run step."),
)
@click.option(
    "-s",
    "--smoke-test",
    required=False,
    default=False,
    is_flag=True,
    help=("Run with every phase of every actor having repeat: 1."),
)
@click.option(
    "-r",
    "--calculate-rollups",
    required=False,
    default=False,
    is_flag=True,
    help=("Whether to automatically calculate rollups from all created FTDC files. "),
)
@click.option(
    "-b",
    "--debug",
    required=False,
    default=False,
    is_flag=True,
    help=(
        "Useful when running genny_core with a debugger. "
        "Does all the poplar/curator/preprocessing stuff but then hangs indefinitely."
    ),
)
@click.pass_context
def workload(
    ctx: click.Context,
    workload_yaml: Tuple[str],
    mongo_uri: str,
    mongostream_uri: Optional[str],
    verbosity: str,
    override: str,
    dry_run: bool,
    smoke_test: bool,
    calculate_rollups: bool,
    debug: bool,
):
    from genny.tasks import genny_runner

    ctx.ensure_object(dict)

    genny_runner.main_genny_runner(
        workload_yaml_path=workload_yaml[0],
        mongo_uri=mongo_uri,
        mongostream_uri=mongostream_uri,
        verbosity=verbosity,
        override=override,
        dry_run=dry_run,
        smoke_test=smoke_test,
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        cleanup_metrics=True,
        hang=debug,
        should_calculate_rollups=calculate_rollups,
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
@click.option(
    "-w",
    "--workload",
    required=False,
    default=None,
    help=("Workload to dry-run."),
)
@click.pass_context
def dry_run_workloads(ctx: click.Context, workload: str):
    from genny.tasks import dry_run

    dry_run.dry_run_workloads(
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        given_workload=workload,
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
        exists=False,
        file_okay=False,
        dir_okay=True,
        writable=True,
        resolve_path=True,
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
        genny_repo_root=ctx.obj["GENNY_REPO_ROOT"],
        actor_name=actor_name,
    )


@cli.command(
    "generate-uuid-tag",
    help=("Generate a random UUID tag for headers."),
)
@click.pass_context
def generate_uuid_tag(ctx: click.Context):
    from genny.tasks import generate_uuid_tag

    generate_uuid_tag.run_generate_uuid_tag(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"])


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
@click.option(
    "--format/--no-format",
    "lint_format",
    is_flag=True,
    help="Do not lint the yaml formatting.",
    default=False,
    type=bool,
)
@click.pass_context
def lint_yaml(ctx: click.Context, lint_format: bool):
    from genny.tasks.yaml_linter import YamlLinter

    YamlLinter(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"]).lint(lint_format=lint_format)


@cli.command(name="generate-docs", help="Generate documentation for all workloads.")
@click.pass_context
def generate_docs(ctx: click.Context):
    from genny.tasks.documentation_generator import DocumentationGenerator

    DocumentationGenerator(genny_repo_root=ctx.obj["GENNY_REPO_ROOT"]).generate()


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
    "--tasks",
    required=True,
    type=click.Choice(["all_tasks", "variant_tasks", "patch_tasks"]),
)
@click.option(
    "--dry-run",
    required=False,
    default=False,
    is_flag=True,
    help=("Dry-run without generating tasks. Used for testing workload YAML files."),
)
@click.pass_context
def auto_tasks(ctx: click.Context, tasks: str, dry_run: bool):
    from genny.tasks import auto_tasks

    auto_tasks.main(
        mode_name=tasks,
        dry_run=dry_run,
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
    )


@cli.command(
    name="auto-tasks-all",
    help=(
        "Determine which Genny workloads should be schedulable for a given evergreen yaml config. "
        "Parses the config to find every build configuration."
        "This is used by evergreen and allows new genny workloads to be created "
        "without having to modify any repos outside of genny itself."
    ),
)
@click.option(
    "--project-file",
    "project_files",
    required=True,
    multiple=True,
    help="An evergreen project file, such as system_perf.yml",
)
@click.option(
    "--no-activate", default=False, is_flag=True, help="Ensure that generated tasks don't activate"
)
@click.pass_context
def auto_tasks_all(ctx: click.Context, project_files: List[str], no_activate: bool):
    from genny.tasks import auto_tasks_all

    auto_tasks_all.main(
        project_files=project_files,
        workspace_root=ctx.obj["WORKSPACE_ROOT"],
        no_activate=no_activate,
    )


if __name__ == "__main__":
    sys.argv[0] = "run-genny"
    cli()
