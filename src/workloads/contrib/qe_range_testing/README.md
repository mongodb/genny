# Summary

qe_range_testing is a python program which generates Genny workloads for Queryable Encryption Range.

## Prerequisites

Install Poetry, following by [their documentation](https://python-poetry.org/docs/#installation) for one of the supported installation methods.

Then, using this directory as your current working directory, invoke Poetry to install the required dependencies into your local virtual environment, using the specific versions enumerated in `poetry.lock`.

```sh
poetry install
```

## General Usage

Note: In order to generate workloads which are locally testable, you will have to edit the `MONGO_CRYPT_PATH` constant in `experiment_generator.py` to point to `mongo_crypt_v1.so` on your local machine. The easiest way to get this file is to run the `crypt_create_lib` task in the `Amazon Linux 2 arm64 Crypt Compile` build variant and download the resulting shared library file. Otherwise, you'll have to compile the library yourself; instructions not attached. This is not required if you are only generating workloads to be run on Evergreen.

Generate tests and runtime requirements by invoking the experiment generator as follows:

```sh
poetry run python experiment_generator.py
```

This command will create:

- `data/*.txt` :: Files containing the data to be inserted by the workloads.
- `queries/*.txt` :: Files containing the queries to be run by the workloads.
- `workloads/*/*.yml` :: Each of these files represent a workload to be run by Genny. `workloads/local/*.yml` are intended for running locally, while `workloads/evergreen/*.yml` are intended for running in Evergreen.
- `generated/rc_perfconfig.yml` :: *Most* of a YAML file which perf_tooling can use to download performance metrics. At a minimum, you'll need to edit in the Patch ID from an Evergreen patch you put up.

The `cleanup.sh` shell script will remove all of these generated objects.

The `setup-encrypted-workloads.sh` shell script will copy a subset of the generated workloads into the main `workloads/encrypted` directory. This is where QE range testing workloads intended to be committed go. These workloads still rely on the `queries/` and `data/` subdirectories in this directory. However, it's worth noting that not all of the query files are used by the subset of committed workloads -- `queries/*_t1_*` and `queries/*_t2_*` are not used, and thus should not be committed.
