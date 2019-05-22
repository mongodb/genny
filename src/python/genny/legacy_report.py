"""
Parse cedar-csv output into legacy "perf.json" report file format.
"""
import argparse
import json
import sys

from genny.parsers.csv2 import CSV2, IntermediateCSVColumns


def build_parser():
    parser = argparse.ArgumentParser(description='Convert cedar-csv output into legacy perf.json'
                                                 'report file format')
    parser.add_argument('--report-file', default='perf.json', help='path to the perf.json report file')
    parser.add_argument('input_file', metavar='input-file', help='path to genny csv2 perf data')

    return parser


class _LegacyReportIntermediateFormat(object):
    def __init__(self):
        self.duration_sum = 0
        self.n = 0
        self.threads = set()
        self.started = sys.maxsize
        self.ended = 0
        self.ops_per_ns = 0

    def finalize(self):
        self.ops_per_ns = self.n / (self.ended - self.started)
        return self

    def _asdict(self):
        # Name derives from the _asdict method of colllections.namedtuple
        return {
            'started': self.started,
            'ended': self.ended,
            'ops_per_ns': self.ops_per_ns,
            'threads': self.threads
        }


def _translate_to_perf_json(timers):
    out = []
    for name, timer in timers.items():
        out.append({
            'name': name,
            'workload': name,
            'start': timer['started'] / 100000,
            'end': timer['ended'] / 100000,
            'results': {
                len(timer['threads']): {
                    'ops_per_sec': timer['ops_per_ns'] * 1e9,
                    'ops_per_sec_values': [timer['ops_per_ns'] * 1e9]
                }
            }
        })
    return {'results': out}


def run(args):
    timers = {}

    my_csv2 = CSV2(args.input_file)

    with my_csv2.data_reader() as data_reader:
        metric_name = None
        report = None

        for line, actor in data_reader:
            cur_metric_name = '{}-{}'.format(actor, line[IntermediateCSVColumns.OPERATION])

            if cur_metric_name != metric_name:
                if report:
                    # pylint: disable=protected-access
                    timers[metric_name] = report.finalize()._asdict()
                metric_name = cur_metric_name
                report = _LegacyReportIntermediateFormat()

            end_metric_time = line[IntermediateCSVColumns.TS]
            duration = line[IntermediateCSVColumns.DURATION]
            end_system_time = my_csv2.metric_to_system_time_ns(end_metric_time)
            start_system_time = end_system_time - duration

            report.threads.add(line[IntermediateCSVColumns.THREAD])
            report.started = min(report.started, start_system_time)
            report.ended = max(report.ended, end_system_time)
            report.duration_sum += duration
            report.n += 1

        if report:
            # pylint: disable=protected-access
            timers[metric_name] = report.finalize()._asdict()

    result = _translate_to_perf_json(timers)

    with open(args.report_file, 'w') as f:
        json.dump(result, f)


def main__legacy_report(argv=sys.argv[1:]):
    """
    Entry point to convert cedar csv metrics format into legacy perf.json
    :return: None
    """
    parser = build_parser()
    run(parser.parse_args(argv))
