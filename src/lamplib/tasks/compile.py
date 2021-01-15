import structlog
import os
import subprocess

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


# TODO: cmdline_cmake_args
def cmake(build_system: str, os_family: str, linux_distro: str, ignore_toolchain_version: bool, sanitizer: str):
    toolchain_info = toolchain.toolchain_info(os_family, linux_distro, ignore_toolchain_version)

    generators = {"make": "Unix Makefiles", "ninja": "Ninja"}
    cmake_cmd = ["cmake", "-B", "build", "-G", generators[build_system]]
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(
        toolchain_info["toolchain_dir"],
        "installed/x64-{}-shared".format(toolchain_info["triplet_os"])
    )
    cmake_toolchain_file = os.path.join(toolchain_info["toolchain_dir"], "scripts/buildsystems/vcpkg.cmake")

    cmake_cmd += [
        "-DCMAKE_PREFIX_PATH={}".format(cmake_prefix_path),
        "-DCMAKE_TOOLCHAIN_FILE={}".format(cmake_toolchain_file),
        "-DVCPKG_TARGET_TRIPLET=x64-{}-static".format(toolchain_info["triplet_os"]),
    ]

    cmake_cmd += _sanitizer_flags(sanitizer)

    # TODO:
    # cmake_cmd += cmdline_cmake_args

    SLOG.info("Running cmake", cmd=cmake_cmd)
    subprocess.run(cmake_cmd, env=toolchain_info["toolchain_env"])


def compile_all(build_system: str, os_family: str, linux_distro: str, ignore_toolchain_version: bool):
    toolchain_info = toolchain.toolchain_info(os_family, linux_distro, ignore_toolchain_version)
    compile_cmd = [build_system, "-C", "build"]
    SLOG.info("Compiling", compile_cmd=compile_cmd)
    SLOG.debug("Compile env", env=toolchain_info["toolchain_env"])
    subprocess.run(compile_cmd, env=toolchain_info["toolchain_env"])


def install(context, env):
    install_cmd = [context.BUILD_SYSTEM, "-C", "build", "install"]
    SLOG.debug("Running install: %s", " ".join(install_cmd))
    subprocess.run(install_cmd, env=env)


def clean(build_system: str, os_family: str, linux_distro: str, ignore_toolchain_version: bool):
    toolchain_info = toolchain.toolchain_info(os_family, linux_distro, ignore_toolchain_version)
    clean_cmd = [build_system, "-C", "build", "clean"]
    SLOG.info("Running clean", cmd=clean_cmd)
    subprocess.run(clean_cmd, env=toolchain_info["toolchain_env"])

    # Physically remove all built files.
    SLOG.debug("Erasing `build/` and `genny_venv`")
    subprocess.run(["rm", "-rf", "build"], env=toolchain_info["toolchain_env"])
    subprocess.run(["rm", "-rf", "genny_venv"], env=toolchain_info["toolchain_env"])

    # Put back build/.gitinore
    subprocess.run(["git", "checkout", "--", "build"], env=toolchain_info["toolchain_env"])
