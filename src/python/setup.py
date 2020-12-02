# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from setuptools import setup
import os


def _install_requires():
    python_dir = os.path.dirname(__file__)
    reqs_file = os.path.join(python_dir, "requirements.txt")
    with open(reqs_file) as handle:
        return handle.readlines()


setup(
    name="genny",
    version="1.0",
    packages=["gennylib", "gennylib.parsers", "third_party"],
    setup_requires=["nose==1.3.7"],
    install_requires=_install_requires(),
    entry_points={
        "console_scripts": [
            "genny-metrics-legacy-report = gennylib.legacy_report:main__legacy_report",
            "lint-yaml = gennylib.yaml_linter:main",
            "genny = gennylib.genny_runner:main_genny_runner",
            "genny-canaries = gennylib.canaries_runner:main_canaries_runner",
            "genny-auto-tasks = gennylib.auto_tasks:main",
        ]
    },
)
