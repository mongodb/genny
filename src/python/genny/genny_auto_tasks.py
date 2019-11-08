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


class AutoRunSpec():
    """
    AutoRunSpec class encapsulates the 'AutoRun' section of a workload yaml file, if it exists
    """

    def __init__(self, required_dict):
        # A dictionary representing the yaml within the Requires section of the workload AutoRun yaml
        self.required_dict = required_dict

    @staticmethod
    def create_from_workload_yaml(workload_yaml):
        """
        :param dict workload_yaml: dict representation of the workload yaml file
        :return AutoRunSpec: if workload contains a valid AutoRun section: returns an AutoRunSpec containing the requirements for this yaml to be autorun. Else, returns None.
        """
        if 'AutoRun' not in workload_yaml or not isinstance(workload_yaml['AutoRun'], dict):
            return None
        if 'Requires' not in workload_yaml['AutoRun'] or not isinstance(workload_yaml['AutoRun']['Requires'], dict):
            return None

        required_dict = workload_yaml['AutoRun']['Requires']
        for module, config in required_dict.items():
            if not isinstance(config, dict):
                return None

        return AutoRunSpec(required_dict)


def to_snake_case(str):
    """
    Converts str to snake_case, useful for generating test id's
    From: https://stackoverflow.com/questions/1175208/elegant-python-function-to-convert-camelcase-to-snake-case
    :return: snake_case version of str.
    """
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', str)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()


def cd_genny_root():
    """
    Changes the current working directory to the genny repository root, which is required for running the git commands below.
    """
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    # cd into script directory first so we can get the project root with git.
    os.chdir(script_dir)
    root = get_project_root()
    os.chdir(root)


def get_project_root():
    """
    :return: the path of the project root.
    """
    try:
        out = subprocess.check_output('git rev-parse --show-toplevel', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e

    return out.decode().strip()


def modified_workload_files():
    """
    Returns a list of filenames for workloads that have been modified according to git, relative to origin/master.
    :return: a list of filenames in the format subdirectory/Task.yml
    """
    try:
        # Returns the names of files in src/workloads/ that have been added/modified/renamed since the common ancestor of HEAD and origin/master
        out = subprocess.check_output(
            'git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- src/workloads/', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e

    if out.decode() == '':
        return []

    # Make paths relative to workloads/ e.g. src/workloads/scale/NewTask.yml --> scale/NewTask.yml
    short_filenames = [f.split('workloads/', 1)[1] for f in out.decode().strip().split('\n')]
    short_filenames = list(filter(lambda x: x.endswith('.yml'), short_filenames))
    return short_filenames


def workload_should_autorun(autorun_spec, env_dict):
    """
    Check if the given workload's AutoRun conditions are met by the current environment
    :param AutoRunSpec autorun_spec: the workload's requirements to be autorun.
    :param dict env_dict: a dict representing the values from bootstrap.yml and runtime.yml
    :return: True if this workload should be autorun, else False.
    """
    if autorun_spec is None:
        return False

    for module, required_config in autorun_spec.required_dict.items():
        if module not in env_dict:
            return False
        if not isinstance(required_config, dict):
            return False

        # True if set of config key-vaue pairs is subset of env_dict key-value pairs
        # This will be false if the AutoRun yaml uses a list to represent multiple matching criteria, but it is efficient so we use it for a first-pass.
        if not required_config.items() <= env_dict[module].items():
            # Now have to check all k, v pairs individually
            for key, autorun_val in required_config.items():
                if key not in env_dict[module]:
                    return False
                if autorun_val == env_dict[module][key]:
                    continue

                # Values not exactly equal, but the AutoRun value could be a list of possibilities.
                if not isinstance(autorun_val, list):
                    return False
                if env_dict[module][key] not in autorun_val:
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

            autorun_spec = AutoRunSpec.create_from_workload_yaml(workload_dict)
            if workload_should_autorun(autorun_spec, env_dict):
                matching_files.append(fname.split('/src/workloads/')[1])

    return matching_files


def make_env_dict(dirname):
    """
    :param str dir: the directory in which to look for bootstrap.yml and runtime.yml files.
    :return: a dict representation of bootstrap.yml and runtime.yml in the cwd, with top level keys 'bootstrap' and 'runtime'
    """
    env_files = ['bootstrap.yml', 'runtime.yml']
    env_dict = {}
    for fname in env_files:
        fname = os.path.join(dirname, fname)
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
    workload_group.add_argument('--modified', action='store_true', default=False,
                                help='if set, the script will generate tasks for workloads that have been added/modifed locally, relative to origin/master')

    parser.add_argument('--forced-workloads', nargs='+',
                                help='paths of workloads to run, relative to src/workloads/ in the genny repository root')

    args = parser.parse_args(sys.argv[1:])
    original_cwd = os.getcwd()
    cd_genny_root()

    if args.autorun:
        env_dict = make_env_dict(original_cwd)
        if env_dict is None:
            print(
                'fatal error: bootstrap.yml and runtime.yml files not found in current directory, cannot AutoRun workloads\n\
                note: --autorun is intended to be called from within Evergreen. If using genny locally, please run the workload directly.',
                file=sys.stderr)
            print(os.getcwd(), file=sys.stderr)
            return

        workloads = autorun_workload_files(env_dict)
        if len(workloads) == 0:
            raise Exception('No AutoRun workloads found matching environment, generating no tasks.')
    elif args.modified:
        workloads = modified_workload_files()
        if len(workloads) == 0:
            raise Exception(
                'No modified workloads found, generating no tasks.\n\
                No results from command: git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- ../workloads/\n\
                Ensure that any added/modified workloads have been committed locally.')

    if args.forced_workloads is not None:
        errs = validate_user_workloads(args.forced_workloads)
        if len(errs) > 0:
            for e in errs:
                print('invalid workload: {}'.format(e), file=sys.stderr)
            return
        workloads.extend(args.forced_workloads)

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
