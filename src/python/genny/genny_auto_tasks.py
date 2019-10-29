"""
Output json that can be used as input to evergreen's generate.tasks, representing genny workloads to be run
"""
import argparse
import os
import re
import sys
import subprocess
import textwrap

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


def modified_workload_files():
    """
    Returns a list of filenames for workloads that have been modified according to git, relative to origin/master.
    :return: a list of filenames in the format subdirectory/Task.yml
    """
    # Return the names of files in ../workloads/ that have been added/modified/renamed since the common ancestor of HEAD and origin/master
    try:
        out = subprocess.check_output(
            'git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- ../workloads/', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        raise e

    if out.decode() == '':
        return []

    # Make paths relative to workloads/ e.g. ../workloads/scale/NewTask.yml --> scale/NewTask.yml
    short_filenames = [f.split('workloads/', 1)[1] for f in out.decode().strip().split('\n')]
    short_filenames = list(filter(lambda x: x.endswith('.yml'), short_filenames))
    return short_filenames


def get_project_root():
    """
    :return: the path of the project root.
    """
    try:
        out = subprocess.check_output('git rev-parse --show-toplevel', shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        raise e

    return out.decode().strip()


def validate_user_workloads(workloads):
    if len(workloads) == 0:
        return 'No workloads specified'

    genny_root = get_project_root()
    for w in workloads:
        workload_path = '{}/src/workloads/{}'.format(genny_root, w)
        if not os.path.isfile(workload_path):
            return 'no file at path {}'.format(workload_path)
        if not workload_path.endswith('.yml'):
            return '{} is not a .yml workload file'.format(workload_path)

    return None


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
    Main Function: parses args, writes to file evergreen tasks in json format for workloads that have been modified locally.
    """
    parser = argparse.ArgumentParser(
        description="Generates json that can be used as input to evergreen's generate.tasks, representing genny workloads to be run")

    parser.add_argument('--variants', nargs='+', required=True, help='buildvariants that workloads should run on')
    parser.add_argument('--workloads', nargs='+', help='paths of workloads to run, relative to genny/src/workloads/')
    parser.add_argument('-o', '--output', default='build/generated_tasks.json',
                        help='path of file to output result json to, relative to the genny root directory')
    args = parser.parse_args(sys.argv[1:])

    if args.workloads is not None:
        err = validate_user_workloads(args.workloads)
        if err is not None:
            print(err)  # TODO stderr
            return
        workloads = args.workloads
    else:
        workloads = modified_workload_files()
        if len(workloads) == 0:
            print(
                'No modified workloads found, generating no tasks.\n\
                No results from command: git diff --name-only --diff-filter=AMR $(git merge-base HEAD origin/master) -- ../workloads/\n\
                Ensure that any added/modified workloads have been committed locally.')
            return

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
