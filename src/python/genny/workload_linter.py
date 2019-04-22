import logging
import os
import os.path as path
import sys

import yamllint.cli


def main():
    logging.basicConfig(level=logging.INFO)

    if not path.exists('.genny-root'):
        logging.error('Please run this script from the root of the Genny repository')
        sys.exit(1)

    workload_dirs = [
        path.join(os.getcwd(), 'src', 'workloads'),
        path.join(os.getcwd(), 'src', 'phases')
    ]
    workload_files = []
    has_error = False

    for workload_dir in workload_dirs:
        for dirpath, dirnames, filenames in os.walk(workload_dir):
            for filename in filenames:
                if filename.endswith('.yaml'):
                    logging.error('All workload YAML files should have the .yml extension, found %s', filename)
                    # Don't error immediately so all violations can be printed with one run
                    # of this script.
                    has_error = True
                elif filename.endswith('.yml'):
                    workload_files.append(path.join(dirpath, filename))

    if has_error:
        sys.exit(1)

    if len(workload_files) == 0:
        logging.error('Did not find any YAML files to lint in the directories: %s', ' '.join(workload_dirs))
        sys.exit(1)

    config_file_path = path.join(os.getcwd(), '.yamllint')

    yamllint_argv = sys.argv[1:] + [
                        '--strict',
                        '--config-file', config_file_path,
                    ] + workload_files

    print('\nLinting {} Genny workload YAML files with yamllint\n'.format(len(workload_files)))

    logging.debug('Invoking yamllint with the following command: '.join(yamllint_argv))

    yamllint.cli.run(yamllint_argv)
