import os
import sys
import platform

import structlog

SLOG = structlog.get_logger(__name__)


def _check_venv():
    if "VIRTUAL_ENV" not in os.environ:
        SLOG.error("Tried to execute without active virtualenv.")
        raise Exception(f"Tried to execute without active virtualenv in cwd={os.getcwd()}")


def run_self_test(genny_repo_root: str, workspace_root: str):
    _check_venv()
    _validate_python_installation()

    import pytest

    path = os.path.join(workspace_root, "build", "XUnitXML", "PyTest.xml")
    os.makedirs(os.path.dirname(path), exist_ok=True)

    # TODO: https://docs.pytest.org/en/stable/deprecations.html#junit-family-default-value-change-to-xunit2
    args = ["--junit-xml", path, genny_repo_root]

    SLOG.info("Running pytest.", args=args)
    pytest.main(args)


def _python_version_string():
    return ".".join(map(str, sys.version_info))[0:5]


def _validate_python_installation():
    # Check Python version
    if not sys.version_info >= (3, 7):
        raise OSError(
            f"Detected Python version {_python_version_string()} less than 3.7. Please delete "
            "the virtualenv and run lamp again."
        )

    # Check the macOS version. Non-mac platforms return a tuple of empty strings
    # for platform.mac_ver().
    if platform.mac_ver()[0] == "10":
        release_triplet = platform.mac_ver()[0].split(".")
        if int(release_triplet[1]) < 14:
            # You could technically compile clang or gcc yourself on an older version
            # of macOS, but it's untested so we might as well just enforce
            # a blanket minimum macOS version for simplicity.
            SLOG.error(
                "Genny requires macOS 10.14 Mojave or newer.", release_triplet=release_triplet
            )
            raise Exception(f"Unknown macOS release triplet {release_triplet}")
