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


Script: `genny-metrics-summarize`
---------------------------------

Produces a summary in JSON format given the 'csv' output format:

```sh
./genny -w InsertRemove.yml -o metrics.csv
genny-metrics-summarize < metrics.csv
```

