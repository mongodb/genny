import logging
import os
import sys
import subprocess

def _sanitizer_flags(context):
    if context.SANITIZER is None:
        return []

    if context.SANITIZER == 'asan':
        return ['-DCMAKE_CXX_FLAGS=-pthread -fsanitize=address -O1 -fno-omit-frame-pointer -g']
    elif context.SANITIZER == 'tsan':
        return ['-DCMAKE_CXX_FLAGS=-pthread -fsanitize=thread -g -O1']
    elif context.SANITIZER == 'ubsan':
        return ['-DCMAKE_CXX_FLAGS=-pthread -fsanitize=undefined -g -O1']

    # arg parser should prevent us from getting here
    raise ValueError('Unknown sanitizer {}'.format(context.SANITIZER))

def cmake(context, toolchain_dir, env, cmdline_cmake_args):
    generators = {
        'make': 'Unix Makefiles',
        'ninja': 'Ninja'
    }
    cmake_cmd = ['cmake', '-B', 'build', '-G', generators[context.BUILD_SYSTEM]]
    # We set both the prefix path and the toolchain file here as a hack to allow cmake
    # to find both shared and static libraries. vcpkg doesn't natively support a project
    # using both.
    cmake_prefix_path = os.path.join(toolchain_dir, 'installed/x64-{}-shared'.format(context.TRIPLET_OS))
    cmake_toolchain_file = os.path.join(toolchain_dir, 'scripts/buildsystems/vcpkg.cmake')

    cmake_cmd += [
        '-DCMAKE_PREFIX_PATH={}'.format(cmake_prefix_path),
        '-DCMAKE_TOOLCHAIN_FILE={}'.format(cmake_toolchain_file),
        '-DVCPKG_TARGET_TRIPLET=x64-{}-static'.format(context.TRIPLET_OS),
    ]

    cmake_cmd += _sanitizer_flags(context)

    cmake_cmd += cmdline_cmake_args

    logging.info('Running cmake: %s', ' '.join(cmake_cmd))
    subprocess.run(cmake_cmd, env=env)

def compile_all(context, env):
    compile_cmd = [context.BUILD_SYSTEM, '-C', 'build']
    logging.info('Compiling: %s', ' '.join(compile_cmd))
    subprocess.run(compile_cmd, env=env)


def install(context, env):
    install_cmd = [context.BUILD_SYSTEM, '-C', 'build', 'install']
    logging.info('Running install: %s', ' '.join(install_cmd))
    subprocess.run(install_cmd, env=env)

    # Install python scripts
    python_cmd = ['python3', 'setup.py', 'develop']
    subprocess.run(python_cmd, cwd='src/python')


def clean(context, env):
    clean_cmd = [context.BUILD_SYSTEM, '-C', 'build', 'clean']
    logging.info('Running clean: %s', ' '.join(clean_cmd))
    subprocess.run(clean_cmd, env=env)

    # Physically remove all built files.
    logging.info('Erasing `build/` and `dist/`')
    subprocess.run(['rm', '-rf', 'build'], env=env)
    subprocess.run(['rm', '-rf', 'dist'], env=env)

    # Put back build/.gitinore
    subprocess.run(['git', 'checkout', '--', 'build'], env=env)
