import logging
import os
import subprocess


def cmake(toolchain_dir, cmdline_args, cmdline_cmake_args, triplet_os, env):
    generators = {
        'make': 'Unix Makefiles',
        'ninja': 'Ninja'
    }
    cmake_cmd = ['cmake', '-B', 'build', '-G', generators[cmdline_args.build_system]]
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(toolchain_dir, 'installed/x64-{}-shared'.format(triplet_os))
    cmake_toolchain_file = os.path.join(toolchain_dir, 'scripts/buildsystems/vcpkg.cmake')

    cmake_cmd += [
        '-DCMAKE_PREFIX_PATH={}'.format(cmake_prefix_path),
        '-DCMAKE_TOOLCHAIN_FILE={}'.format(cmake_toolchain_file),
        '-DVCPKG_TARGET_TRIPLET=x64-{}-static'.format(triplet_os),
    ]

    cmake_cmd += cmdline_cmake_args

    logging.info('Running cmake: %s', ' '.join(cmake_cmd))
    subprocess.run(cmake_cmd, env=env)


def compile_all(env, args):
    compile_cmd = [args.build_system, '-C', 'build']
    logging.info('Compiling: %s', ' '.join(compile_cmd))
    subprocess.run(compile_cmd, env=env)


def install(env, args):
    install_cmd = [args.build_system, '-C', 'build', 'install']
    logging.info('Running install: %s', ' '.join(install_cmd))
    subprocess.run(install_cmd, env=env)
