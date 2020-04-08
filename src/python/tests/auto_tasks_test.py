import sys
import json
from subprocess import CalledProcessError
import unittest
from typing import NamedTuple, Dict, List, Tuple

from unittest.mock import patch, mock_open
from unittest.mock import Mock

from gennylib import auto_tasks as at


class Example(NamedTuple):
    given_expansions: Dict[str, str]
    given_args: List[str]
    expect_output: Tuple[str, dict]


class AutoTasksTests(unittest.TestCase):

    def test_foo(self):
        at.main(["some-foo.py", "all"])

