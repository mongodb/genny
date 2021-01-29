from typing import List

import structlog
import os
from cmd_runner import run_command

import toolchain

SLOG = structlog.get_logger(__name__)


def _sanitizer_flags(sanitizer: str = None):
    if sanitizer is None:
        return []

    if sanitizer == "asan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"]
    elif sanitizer == "tsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=thread -g -O1"]
    elif sanitizer == "ubsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=undefined -g -O1"]

    # arg parser should prevent us from getting here
    raise ValueError("Unknown sanitizer {}".format(sanitizer))


def cmake(
    genny_repo_root: str,
    workspace_root: str,
    build_system: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
    sanitizer: str,
    cmake_args: List[str],
):
    toolchain_info = toolchain.toolchain_info(
        workspace_root=workspace_root,
        genny_repo_root=genny_repo_root,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )

    generators = {"make": "Unix Makefiles", "ninja": "Ninja"}
    cmake_cmd = ["cmake", "-B", "build", "-G", generators[build_system]]
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(
        toolchain_info.toolchain_dir, f"installed/x64-{toolchain_info.triplet_os}-shared",
    )
    cmake_toolchain_file = os.path.join(
        toolchain_info.toolchain_dir, "scripts/buildsystems/vcpkg.cmake"
    )

    cmake_cmd += [
        "-DCMAKE_PREFIX_PATH={}".format(cmake_prefix_path),
        "-DCMAKE_TOOLCHAIN_FILE={}".format(cmake_toolchain_file),
        f"-DVCPKG_TARGET_TRIPLET=x64-{toolchain_info.triplet_os}-static",
    ]

    cmake_cmd += _sanitizer_flags(sanitizer)

    cmake_cmd += cmake_args

    run_command(
        cwd=genny_repo_root,
        cmd=cmake_cmd,
        env=toolchain_info.toolchain_env,
        capture=False,
        check=True,
    )


def compile_all(
    genny_repo_root: str,
    workspace_root: str,
    build_system: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
):
    toolchain_info = toolchain.toolchain_info(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
    compile_cmd = [build_system, "-C", "build"]
    run_command(
        cmd=compile_cmd,
        env=toolchain_info.toolchain_env,
        cwd=genny_repo_root,
        capture=False,
        check=True,
    )


def install(
    genny_repo_root: str,
    workspace_root: str,
    build_system: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
):
    toolchain_info = toolchain.toolchain_info(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
    install_cmd = [build_system, "-C", "build", "install"]
    run_command(
        cmd=install_cmd,
        env=toolchain_info.toolchain_env,
        cwd=genny_repo_root,
        capture=False,
        check=True,
    )


def clean(genny_repo_root: str):
    # Physically remove all built files.
    SLOG.debug("Erasing build, genny_venv, and dist", cwd=os.getcwd())

    def _run_command(cmd):
        run_command(cmd=cmd, cwd=genny_repo_root, capture=False, check=True)

    _run_command(cmd=["rm", "-rf", "build"])
    _run_command(cmd=["rm", "-rf", "genny_venv"])
    _run_command(cmd=["rm", "-rf", "dist"])
    # Put back build/.gitinore
    _run_command(cmd=["git", "checkout", "--", "build"])


def compile_and_install(
    genny_repo_root: str,
    workspace_root: str,
    build_system: str,
    os_family: str,
    linux_distro: str,
    ignore_toolchain_version: bool,
    sanitizer: str,
    cmake_args: List[str],
):
    cmake(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        build_system=build_system,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
        sanitizer=sanitizer,
        cmake_args=cmake_args,
    )
    compile_all(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        build_system=build_system,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
    install(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        build_system=build_system,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
