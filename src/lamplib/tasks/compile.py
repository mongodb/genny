import logging
import os
import sys
import subprocess


def _sanitizer_flags(context):
    if context.SANITIZER is None:
        return []

    if context.SANITIZER == "asan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g"]
    elif context.SANITIZER == "tsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=thread -g -O1"]
    elif context.SANITIZER == "ubsan":
        return ["-DCMAKE_CXX_FLAGS=-pthread -fsanitize=undefined -g -O1"]

    # arg parser should prevent us from getting here
    raise ValueError("Unknown sanitizer {}".format(context.SANITIZER))


def cmake(context, toolchain_dir, env, cmdline_cmake_args):
    generators = {"make": "Unix Makefiles", "ninja": "Ninja"}
    cmake_cmd = ["cmake", "-B", "build", "-G", generators[context.BUILD_SYSTEM]]
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(
        toolchain_dir, "installed/x64-{}-shared".format(context.TRIPLET_OS)
    )
    cmake_toolchain_file = os.path.join(toolchain_dir, "scripts/buildsystems/vcpkg.cmake")

    cmake_cmd += [
        "-DCMAKE_PREFIX_PATH={}".format(cmake_prefix_path),
        "-DCMAKE_TOOLCHAIN_FILE={}".format(cmake_toolchain_file),
        "-DVCPKG_TARGET_TRIPLET=x64-{}-static".format(context.TRIPLET_OS),
    ]

    cmake_cmd += _sanitizer_flags(context)

    cmake_cmd += cmdline_cmake_args

    logging.info("Running cmake: %s", " ".join(cmake_cmd))
    subprocess.run(cmake_cmd, env=env)


def compile_all(context, env):
    compile_cmd = [context.BUILD_SYSTEM, "-C", "build"]
    logging.debug("Compiling: %s", " ".join(compile_cmd))
    subprocess.run(compile_cmd, env=env)


def install(context, env):
    install_cmd = [context.BUILD_SYSTEM, "-C", "build", "install"]
    logging.debug("Running install: %s", " ".join(install_cmd))
    subprocess.run(install_cmd, env=env)


def clean(context, env):
    clean_cmd = [context.BUILD_SYSTEM, "-C", "build", "clean"]
    logging.debug("Running clean: %s", " ".join(clean_cmd))
    subprocess.run(clean_cmd, env=env)

    # Physically remove all built files.
    logging.debug("Erasing `build/` and `genny_venv`")
    subprocess.run(["rm", "-rf", "build"], env=env)
    subprocess.run(["rm", "-rf", "genny_venv"], env=env)

    # Put back build/.gitinore
    subprocess.run(["git", "checkout", "--", "build"], env=env)

# Map of platform.system() to vcpkg's OS names.
_triplet_os_map = {"Darwin": "osx", "Linux": "linux", "NT": "windows"}

# Define complex operations as private methods on the module to keep the
# public Context object clean.
def _create_compile_environment(triplet_os, toolchain_dir):
    env = os.environ.copy()
    paths = [env["PATH"]]

    # For mongodbtoolchain compiler (if there).
    paths.insert(0, "/opt/mongodbtoolchain/v3/bin")

    # For cmake and ctest
    cmake_bin_relative_dir = {
        "linux": "downloads/tools/cmake-3.13.3-linux/cmake-3.13.3-Linux-x86_64/bin",
        "osx": "downloads/tools/cmake-3.13.3-osx/cmake-3.13.3-Darwin-x86_64/CMake.app/Contents/bin",
    }[triplet_os]
    paths.insert(0, os.path.join(toolchain_dir, cmake_bin_relative_dir))

    # For ninja
    ninja_bin_dir = os.path.join(
        toolchain_dir, "downloads/tools/ninja-1.8.2-{}:".format(triplet_os)
    )
    paths.insert(0, ninja_bin_dir)

    env["PATH"] = ":".join(paths)
    env["NINJA_STATUS"] = "[%f/%t (%p) %es] "  # make the ninja output even nicer

    logging.debug("Using environment: %s", env)
    return env


class Context:

    # Command line configurable and runtime values.
    IGNORE_TOOLCHAIN_VERSION = False
    TRIPLET_OS = None
    BUILD_SYSTEM = None
    SANITIZER = None

    # Dynamic context
    _compile_environment = None

    @staticmethod
    def get_compile_environment(toolchain_dir=None):
        if not Context._compile_environment:
            if not toolchain_dir:
                raise ValueError(
                    "toolchain_dir must be specified when getting the compile environment for the "
                    "first time"
                )
            Context._compile_environment = _create_compile_environment(
                Context.TRIPLET_OS, toolchain_dir
            )
        return Context._compile_environment

    # Helper methods
    @staticmethod
    def set_triplet_os(os_family):
        Context.TRIPLET_OS = _triplet_os_map[os_family]