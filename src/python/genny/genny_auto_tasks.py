"""
Output json that can be used as input to evergreen's generate.tasks, representing genny workloads to be run
"""
import argparse
import os
import re
import sys
import subprocess
import yaml
import glob

from shrub.config import Configuration
from shrub.command import CommandDefinition
from shrub.variant import TaskSpec


def to_snake_case(str):
    """
    Converts str to snake_case, useful for generating test id's
    From: https://stackoverflow.com/questions/1175208/elegant-python-function-to-convert-camelcase-to-snake-case
    :return: snake_case version of str.
    """
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', str)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()

def cd_python_dir():
    """
    Changes the current working directory to genny/src/python, which is required for running the git commands below.
    """
    script_path = os.path.abspath(__file__)
    genny_dir = os.path.dirname(script_path)
    python_path = os.path.join(genny_dir, '..')
    os.chdir(python_path)

def get_project_root():
    """
    :return: the path of the project root.
    """

    # Temporarily change the cwd, but we undo this in the finally block.
    original_cwd = os.getcwd()
    cd_python_dir()

    try:
        out = subprocess.check_output('git rev-parse --show-toplevel', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e
    finally:
        os.chdir(original_cwd)

    return out.decode().strip()


def modified_workload_files():
    """
    Returns a list of filenames for workloads that have been modified according to git, relative to origin/master.
    :return: a list of filenames in the format subdirectory/Task.yml
    """

    # Temporarily change the cwd, but we undo this in the finally block.
    original_cwd = os.getcwd()
    cd_python_dir()
    try:
        # Returns the names of files in ../workloads/ that have been added/modified/renamed since the common ancestor of HEAD and origin/master
        out = subprocess.check_output(
            'git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- ../workloads/', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e
    finally:
        os.chdir(original_cwd)

    if out.decode() == '':
        return []

    # Make paths relative to workloads/ e.g. ../workloads/scale/NewTask.yml --> scale/NewTask.yml
    short_filenames = [f.split('workloads/', 1)[1] for f in out.decode().strip().split('\n')]
    short_filenames = list(filter(lambda x: x.endswith('.yml'), short_filenames))
    return short_filenames


def workload_should_autorun(workload_dict, env_dict):
    """
    Check if the given workload's AutoRun conditions are met by the current environment
    :param dict workload_dict: a dict representation of the workload files's yaml.
    :param dict env_dict: a dict representing the values from bootstrap.yml and runtime.yml
    :return: True if this workload should be autorun, else False.
    """

    # First check that the workload has a proper AutoRun section.
    if 'AutoRun' not in workload_dict or not isinstance(workload_dict['AutoRun'], dict):
        return False
    if 'Requires' not in workload_dict['AutoRun'] or not isinstance(workload_dict['AutoRun']['Requires'], dict):
        return False

    for module, required_config in workload_dict['AutoRun']['Requires'].items():
        if module not in env_dict:
            return False
        if not isinstance(required_config, dict):
            return False

        # True if set of config key-vaue pairs is subset of env_dict key-value pairs
        # This will be false if the AutoRun yaml uses a logical or (i.e. branch_name: master | prod), but it is efficient so we use it for a first-pass.
        # if not required_config.items() <= env_dict[module].items():
        if True:
            # Now have to check all k, v pairs individually
            for k, v in required_config.items():
                if k not in env_dict[module]:
                    return False

                bools = [s.strip() == env_dict[module][k] for s in v.split('|')]
                if True not in bools:
                    return False

    return True

def autorun_workload_files(env_dict):
    """
    :param dict env_dict: a dict representing the values from bootstrap.yml and runtime.yml -- the output of make_env_dict().
    :return: a list of workload files whose AutoRun critera are met by the env_dict.
    """
    workload_dir = '{}/src/workloads'.format(get_project_root())
    candidates = glob.glob('{}/**/*.yml'.format(workload_dir), recursive=True)

    matching_files = []
    for fname in candidates:
        with open(fname, 'r') as handle:
            try:
                workload_dict = yaml.safe_load(handle)
            except Exception as e:
                continue
            if workload_should_autorun(workload_dict, env_dict):
                matching_files.append(fname)

    return matching_files


def make_env_dict():
    """
    :return: a dict representation of bootstrap.yml and runtime.yml in the cwd, with top level keys 'bootstrap' and 'runtime'
    """
    env_files = ['bootstrap.yml', 'runtime.yml']
    env_dict = {}
    for fname in env_files:
        if not os.path.isfile(fname):
            return None
        with open(fname, 'r') as handle:
            config = yaml.safe_load(handle)
            module = os.path.basename(fname).split('.yml')[0]
            env_dict[module] = config
    return env_dict


def validate_user_workloads(workloads):
    if len(workloads) == 0:
        return ['No workloads specified']

    errors = []
    genny_root = get_project_root()
    for w in workloads:
        workload_path = '{}/src/workloads/{}'.format(genny_root, w)
        if not os.path.isfile(workload_path):
            errors.append('no file at path {}'.format(workload_path))
            continue
        if not workload_path.endswith('.yml'):
            errors.append('{} is not a .yml workload file'.format(workload_path))

    return errors

def construct_task_json(workloads, variants):
    """
    :param list workloads: a list of filenames of workloads to generate tasks for, each in the format subdirectory/Task.yml
    :param list variants: a list of buildvariants (strings) that the generated tasks should be run on.
    :return: json representation of tasks that run the given workloads, that can be provided to evergreen's generate.tasks command.
    """
    task_specs = []
    c = Configuration()

    for fname in workloads:
        basename = os.path.basename(fname)
        base_parts = os.path.splitext(basename)
        if base_parts[1] != '.yml':
            # Not a .yml workload file, ignore it.
            continue

        task_name = to_snake_case(base_parts[0])
        task_specs.append(TaskSpec(task_name))
        t = c.task(task_name)
        t.priority(5)  # The default priority in system_perf.yml
        t.commands([
            CommandDefinition().function('prepare environment').vars({
                'test': task_name,
                'auto_workload_path': fname
            }),
            CommandDefinition().function('deploy cluster'),
            CommandDefinition().function('run test'),
            CommandDefinition().function('analyze'),
        ])

    for v in variants:
        c.variant(v).tasks(task_specs)

    return c.to_json()


def main():
    """
    Main Function: parses args, writes to file evergreen tasks in json format for workloads that are specified in args.
    """
    parser = argparse.ArgumentParser(
        description="Generates json that can be used as input to evergreen's generate.tasks, representing genny workloads to be run")

    parser.add_argument('--variants', nargs='+', required=True, help='buildvariants that workloads should run on')
    parser.add_argument('-o', '--output', default='build/generated_tasks.json',
                        help='path of file to output result json to, relative to the genny root directory')

    workload_group = parser.add_mutually_exclusive_group(required=True)
    workload_group.add_argument('--autorun', action='store_true', default=False,
                        help='if set, the script will generate tasks based on workloads\' AutoRun section and bootstrap.yml/runtime.yml files in the working directory')
    workload_group.add_argument('--modified', action='store_true', default=False, help='if set, the script will generate tasks for workloads that have been added/modifed locally, relative to origin/master')
    workload_group.add_argument('--workloads', nargs='+',
                        help='paths of workloads to run, relative to src/workloads/ in the genny repository root')

    args = parser.parse_args(sys.argv[1:])

    if args.autorun:
        env_dict = make_env_dict()
        if env_dict is None:
            print('fatal error: bootstrap.yml and runtime.yml files not found in current directory, cannot AutoRun workloads')
            return

        workloads = autorun_workload_files(env_dict)
        if len(workloads) == 0:
            print('No AutoRun workloads found matching environment, generating no tasks.')
            return
    elif args.modified:
        workloads = modified_workload_files()
        if len(workloads) == 0:
            print(
                'No modified workloads found, generating no tasks.\n\
                No results from command: git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- ../workloads/\n\
                Ensure that any added/modified workloads have been committed locally.')
            return
    elif args.workloads is not None:
        errs = validate_user_workloads(args.workloads)
        if len(errs) > 0:
            for e in errs:
                print('invalid workload: {}'.format(err), file=sys.stderr)
            return
        workloads = args.workloads

    task_json = construct_task_json(workloads, args.variants)
    if args.output == 'stdout':
        print(task_json)
        return

    # Interpret args.output relative to the genny root directory.
    project_root = get_project_root()
    output_path = '{}/{}'.format(project_root, args.output)
    with open(output_path, 'w') as output_file:
        output_file.write(task_json)

    print('Wrote generated task json to {}'.format(output_path))


if __name__ == '__main__':
    main()
