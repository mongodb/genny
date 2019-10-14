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
	out = subprocess.check_output('git diff --name-only `git merge-base HEAD origin/master` -- ../workloads/', shell=True)
	# Make paths relative to workloads/ e.g. ../workloads/scale/NewTask.yml --> scale/NewTask.yml
	short_filenames = [f.split('workloads/', 1)[1] for f in out.decode().strip().split('\n')]
	return short_filenames

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
		t.priority(5) # The default priority in system_perf.yml
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
	Main Function: outputs evergreen tasks in json format for workloads that have been modified locally.
	"""
	workloads = modified_workload_files()
	task_json = construct_task_json(workloads, sys.argv[1:])
	print(task_json)

if __name__ == '__main__':
	main()
