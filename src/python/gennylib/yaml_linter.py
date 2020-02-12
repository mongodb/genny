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

    yaml_dirs = [
        path.join(os.getcwd(), 'src', 'workloads'),
        path.join(os.getcwd(), 'src', 'phases'),
        path.join(os.getcwd(), 'src', 'resmokeconfig')
    ]

    yaml_files = [path.join(os.getcwd(), 'evergreen.yml')]

    has_error = False

    for yaml_dir in yaml_dirs:
        for dirpath, dirnames, filenames in os.walk(yaml_dir):
            for filename in filenames:
                if filename.endswith('.yaml'):
                    logging.error('All YAML files should have the .yml extension, found %s',
                                  filename)
                    # Don't error immediately so all violations can be printed with one run
                    # of this script.
                    has_error = True
                elif filename.endswith('.yml'):
                    yaml_files.append(path.join(dirpath, filename))

    if has_error:
        sys.exit(1)

    if len(yaml_files) == 0:
        logging.error('Did not find any YAML files to lint in the directories: %s',
                      ' '.join(yaml_dirs))
        sys.exit(1)

    config_file_path = path.join(os.getcwd(), '.yamllint')

    yamllint_argv = sys.argv[1:] + [
        '--strict',
        '--config-file',
        config_file_path,
    ] + yaml_files

    print('Linting {} Genny workload YAML files with yamllint'.format(len(yaml_files)))

    logging.debug('Invoking yamllint with the following command: '.join(yamllint_argv))

    yamllint.cli.run(yamllint_argv)


if __name__ == '__main__':
    main()
