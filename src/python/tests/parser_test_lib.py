import os

import genny.metrics_output_parser as parser


def parse_string(input_str):
    lines = [line.strip() for line in input_str.split("\n")]
    return parser.parse(lines, "InputString").timers()


def parse_fixture(path):
    full_path = os.path.join('.', 'tests', 'fixtures', path + '.txt')
    with open(full_path, 'r') as f:
        return parser.parse(f, full_path).timers()

