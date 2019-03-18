"""
Parse cedar-csv output into legacy "perf.json" report file format.
"""
import argparse
import sys

from genny.csv2 import CSV2


def build_parser():
    parser = argparse.ArgumentParser(description='Convert cedar-csv output into legacy perf.json'
                                                 'report file format')
    parser.add_argument('--report-file', default='perf.json', help='path to the perf.json report file')
    parser.add_argument('input_file', metavar='input-file', help='path to genny csv2 perf data')

    return parser


def run(args):

    with open(args.input_file, 'r') as f:
        my_csv2 = CSV2(args.input_file)

def main__legacy_report(argv=sys.argv[1:]):
    """
    Entry point to convert cedar csv metrics format into legacy perf.json
    :return: None
    """
    parser = build_parser()
    run(parser.parse_args(argv))
