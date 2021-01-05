🧞🐍 Genny Python  🧞🐍
========================

Genny uses Python for glue with other systems and for
doing pre/post-processing of workloads.

Quick-Start
-----------

Genny uses the `.python-version` file to determine
which version of Python to use. Consider using
[pyenv](https://github.com/pyenv/pyenv).

```sh
brew update && brew install pyenv
pyenv install # uses .python-version to determine python version
pyenv rehash
```

Once a suitable python is installed, setup a virtual
environment and install the `genny` package:

```sh
pip install virtualenv
virtualenv venv
source ./venv/bin/activate
pip install .
```

Or if you wish to install and develop the scripts locally at the same time,
you can use `setup.py develop` to avoid having to install after every change:

```sh
python setup.py develop
```


Script: `lint-yaml`
---------------------

```sh
lint-yaml
```

Lint most of the YAML files in the Genny repository,
including workloads, phases, and `evergreen.yml`.

Make you run the script from the root of the Genny repository
(so it can pick up on the `.yamllint` linter config file)


Script: `genny-auto-tasks`
---------------------------------

Generates JSON that can be used as input to evergreen's generate.tasks,
representing either evergreen tasks or buildvariants that run genny workloads.

If run with `--generate-all-tasks`, `genny-auto-tasks` outputs a JSON file 
of task definitions for all workloads in the `/src/workloads` genny directory.
If run with `--variants`, this program outputs a JSON file of buildvariant
definitions that run a subset of workload tasks based on the other arguments.
These two invocations are separate to ensure that no task will be generated more
than once when run on evergreen (which would cause generate.tasks to throw an
error, even across different buildvariants).

Thus, running `genny-auto-tasks` is generally a two step process. First, one
generates all tasks as follows, with the output JSON going to `build/all_tasks.json`
in this case:

```sh
genny-auto-tasks --generate-all-tasks --output build/all_tasks.json
```

Upon using generate.tasks with the outputted JSON, one can then generate JSON
representing buildvariants that run the proper subset of tasks, based either
on git modification history or the evergreen environment (See "Patch-Testing
Genny Changes with Sys-Perf / DSI" in the main README for more info). For example,
these commands write JSON definitions of `linux-1-node-replSet` buildvariants
that run modified/autorun genny workloads.

```sh
genny-auto-tasks --modified --variants linux-1-node-replSet --output build/variants.json
genny-auto-tasks --autorun --variants linux-1-node-replSet --output build/variants.json
```


Running Self-Tests
------------------

```sh
pip install virtualenv
virtualenv venv
source ./venv/bin/activate
pip install .
python setup.py nosetests
```

Python Formatting
-----------------

Genny uses `black` for python formatting.

```sh
pip install virtualenv
virtualenv venv
source ./venv/bin/activate
pip install .
black src/lamplib src/python
```

