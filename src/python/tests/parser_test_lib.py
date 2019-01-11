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
