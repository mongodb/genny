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
