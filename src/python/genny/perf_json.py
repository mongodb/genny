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
                timer['threads']: {
                    'ops_per_sec': timer['mean'],
                    'ops_per_sec_values': [timer['mean']]
                }
            }
        })
    return {'storageEngine': 'wiredTiger', 'results': out}


def main():
    results = output_parser.parse('/dev/stdin')
    translated = translate(results)
    out = json.dumps(translated, sort_keys=True, indent=4, separators=(',', ': '))
    print(out)


if __name__ == '__main__':
    main()
