# Copyright 2018 MongoDB Inc.
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

"""
Module for interacting with the (legacy) perf.json output format for metrics data.
"""

import json

import genny.metrics_output_parser as output_parser


def translate(parser_results):
    out = []
    for name, timer in parser_results.timers().items():
        out.append({
            'name': name,
            'workload': name,
            'start': timer['started'] / 100000,
            'end': timer['ended'] / 100000,
            'results': {
                len(timer['threads']): {
                    'ops_per_sec': timer['mean'],
                    'ops_per_sec_values': [timer['mean']]
                }
            }
        })
    return {'results': out}


def main__summarize_translate():
    """
    Entry point to translate genny csv to (summarized) perf.json
    Entry point: see setup.py
    :return: None
    """
    with open('/dev/stdin', 'r') as f:
        results = output_parser.ParserResults(f, '/dev/stdin')
        translated = translate(results)
        print(json.dumps(translated, sort_keys=True, indent=4, separators=(',', ': ')))
