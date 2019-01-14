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

setup(name='genny',
      version='1.0',
      packages=['genny'],
      install_requires=[
          'nose==1.3.7',
          'yapf==0.24.0',
      ],
      setup_requires=[
          'nose==1.3.7'
      ],
      entry_points={
          'console_scripts': [
              'genny-metrics-summarize = genny.metrics_output_parser:main__sumarize',
              'genny-metrics-to-perf-json = genny.perf_json:main__summarize_translate',
          ]
      },
      )
