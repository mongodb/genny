import os

import genny.metrics_output_parser as parser


def parse_string(input_str):
    """
    :param input_str: input string to feed to metrics_output_parser
    :return: the parser result
    """
    lines = [line.strip() for line in input_str.split("\n")]
    return parser.ParserResults(lines, "InputString")


def parse_fixture(path):
    """
    :param path: file name (no extension) of fixture to load from the fixtures directory.
    :return: metrics_output_parser result from reading the file
    """
    full_path = os.path.join('.', 'tests', 'fixtures', path + '.txt')
    with open(full_path, 'r') as f:
        return parser.ParserResults(f, full_path)
