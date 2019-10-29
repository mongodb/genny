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
representing genny workloads to be run.

For example, if one wanted to write JSON to `build/tasks.json` that would run the big_update and insert_remove workloads on the `linux-1-node-replSet` buildvariant, the following command could be used:

```sh
genny-auto-tasks --workloads scale/BigUpdate.yml scale/InsertRemove.yml --variants linux-1-node-replSet --output build/tasks.json
```

If the `--workloads` flag is not set, the script will instead generate JSON for all workloads that have been added or modified locally (relative to origin/master).


Script: `genny-metrics-legacy-report`
---------------------------------

Produces a summary in legacy JSON format for the Evergreen perf plugin.

```sh
./genny run -o metrics.csv -m cedar-csv InsertRemove.yml
genny-metrics-legacy-report metrics.csv
```


Script: `genny-metrics-report`
---------------------------------

Send metrics to Evergreen's metrics collection service (Cedar).

This code snippet is provided for reference only as Cedar does
not yet support local environments.

```sh
./genny run -o metrics.csv -m cedar-csv InsertRemove.yml
# requires expansions.yml in cwd for cedar parameters
genny-metrics-report metrics.csv
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

Genny uses `yapf` for python formatting.

```sh
pip install virtualenv
virtualenv venv
source ./venv/bin/activate
pip install .
yapf -i --recursive genny tests
```




