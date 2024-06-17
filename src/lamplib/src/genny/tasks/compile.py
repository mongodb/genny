from typing import List

import structlog
import os
import platform
import subprocess

from genny.cmd_runner import run_command
from genny import toolchain

SLOG = structlog.get_logger(__name__)


def _sanitizer_flags(sanitizer: str, genny_repo_root: str):
    if sanitizer is None:
        return []

    if sanitizer == "asan":
        cmake_cxx_flags = " ".join(
            [
                "-DCMAKE_CXX_FLAGS=-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g",
                "-mllvm -asan-use-private-alias=1",  # suppress false odr-violation, clang only
                f"-fsanitize-blacklist={genny_repo_root}/asan.ignorelist",
            ]
        )  # ignore existing ASAN issues.
        return [cmake_cxx_flags, "-DCMAKE_CXX_COMPILER=clang++"]
    elif sanitizer == "tsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=thread -g -O1"]
    elif sanitizer == "ubsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=undefined -g -O1"]

    # arg parser should prevent us from getting here
    raise ValueError("Unknown sanitizer {}".format(sanitizer))


class DistroDetectionError(Exception):
    pass


def _detect_distro_ubuntu(machine, freedesktop_version):
    if freedesktop_version == "22.04":
        if machine == "aarch64":
            return "ubuntu2204_arm64"
        elif machine == "x86_64":
            return "ubuntu2204"
        else:
            raise DistroDetectionError(f"Invalid machine type for Ubuntu 22.04: {machine}")
    elif freedesktop_version == "20.04":
        if machine == "aarch64":
            return "ubuntu2004_arm64"
        elif machine == "x86_64":
            return "ubuntu2004"
        else:
            raise DistroDetectionError(f"Invalid machine type for Ubunutu 20.04: {machine}")
    elif freedesktop_version == "18.04":
        if machine == "x86_64":
            return "ubuntu1804"
        else:
            raise DistroDetectionError(f"Invalid machine type for Ubuntu 18.04: {machine}")
    else:
        raise DistroDetectionError(f"Invalid version for Ubuntu: {freedesktop_version}")


def _detect_distro_rhel(machine, freedesktop_version):
    # We only distinguish between major versions of RHEL, but they include minor versions in their version_id
    if freedesktop_version.startswith("7."):
        if machine == "x86_64":
            return "rhel70"
        else:
            raise DistroDetectionError(f"Invalid machine type for RHEL 7.x: {machine}")
    elif freedesktop_version.startswith("8."):
        if machine == "x86_64":
            return "rhel8"
        else:
            raise DistroDetectionError(f"Invalid machine type for RHEL 8.x: {machine}")
    else:
        raise DistroDetectionError(f"Invalid version for RHEL: {freedesktop_version}")


def _detect_distro_amazon(machine, freedesktop_version):
    if freedesktop_version == "2":
        if machine == "aarch64":
            return "amazon2_arm64"
        elif machine == "x86_64":
            return "amazon2"
        else:
            raise DistroDetectionError(f"Invalid machine type for Amazon 2: {machine}")
    elif freedesktop_version == "2023":
        if machine == "aarch64":
            return "amazon2023_arm64"
        elif machine == "x86_64":
            return "amazon2023"
        else:
            raise DistroDetectionError(f"Invalid machine type for Amazon 2023: {machine}")
    else:
        raise DistroDetectionError(f"Invalid version for Amazon Linux: {freedesktop_version}")


def _freedesktop_os_release():
    """
    Best effort parser of the freedesktop.org os-release file.

    This is a little imprecise, but works well enough for all three Linux
    distros we support.

    This exists to support Python versions less than 3.10. Otherwise it
    can be replaced with platform.freedesktop_os_release().
    """
    result = {}
    with open("/etc/os-release") as os_release_file:
        for line in os_release_file:
            if "=" not in line:
                # Skip blank lines and any other lines that don't obviously set a value.
                continue
            key, value = line.strip("\n").split("=", 1)
            value = value.strip("'\"")
            result[key] = value
    return result


def detect_distro():
    SLOG.info(f"Distro not specified. Detecting distro.")

    system = platform.system()
    if system == "Darwin":
        distro = "not-linux"
    elif system == "Linux":
        freedesktop_release = _freedesktop_os_release()
        freedesktop_id = freedesktop_release["ID"]
        freedesktop_version = freedesktop_release["VERSION_ID"]
        machine = platform.machine()
        if freedesktop_id == "ubuntu":
            distro = _detect_distro_ubuntu(machine, freedesktop_version)
        elif freedesktop_id == "rhel":
            distro = _detect_distro_rhel(machine, freedesktop_version)
        elif freedesktop_id == "amzn":
            distro = _detect_distro_amazon(machine, freedesktop_version)
        else:
            raise DistroDetectionError(f"Unrecognized Linux distro: {freedesktop_id}")
    else:
        raise DistroDetectionError(f"Cannot determine distro for system {platform.system()}")
    SLOG.info(f"Found distro for installation.", distro=distro)
    return distro


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
    cmake_prefix_paths = [
        os.path.join(
            toolchain_info.toolchain_dir,
            f"installed/{toolchain_info.triplet_arch}-{toolchain_info.triplet_os}-release",
        ),
        os.path.join(
            toolchain_info.toolchain_dir,
            f"installed/{toolchain_info.triplet_arch}-{toolchain_info.triplet_os}-dynamic",
        ),
    ]

    cmake_cmd += [
        "-DGENNY_WORKSPACE_ROOT={}".format(workspace_root),
        "-DGENNY_TOOLCHAIN_DIR={}".format(toolchain_info.toolchain_dir),
        "-DCMAKE_PREFIX_PATH='{}'".format(";".join(cmake_prefix_paths)),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=1",
        f"-DVCPKG_TARGET_TRIPLET={toolchain_info.triplet_arch}-{toolchain_info.triplet_os}-release",
    ]

    cmake_cmd += _sanitizer_flags(sanitizer, genny_repo_root)

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
    cmd_base = [build_system]
    if build_system == "make":
        cmd_base.append("-j8")
    compile_cmd = [*cmd_base, "-C", "build"]
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
    def _run_command(cmd):
        run_command(cmd=cmd, cwd=genny_repo_root, capture=False, check=True)

    # Physically remove all built files.
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

    try:
        compile_all(
            genny_repo_root=genny_repo_root,
            workspace_root=workspace_root,
            build_system=build_system,
            os_family=os_family,
            linux_distro=linux_distro,
            ignore_toolchain_version=ignore_toolchain_version,
        )

    except subprocess.CalledProcessError as ex:
        SLOG.critical(
            "Genny compile has failed. This is sometimes caused by having an old mongodbtoolchain. To update: curl -o toolchain_installer.sh http://mongodbtoolchain.build.10gen.cc/installer.sh && bash toolchain_installer.sh"
        )
        raise

    except:
        raise

    install(
        genny_repo_root=genny_repo_root,
        workspace_root=workspace_root,
        build_system=build_system,
        os_family=os_family,
        linux_distro=linux_distro,
        ignore_toolchain_version=ignore_toolchain_version,
    )
