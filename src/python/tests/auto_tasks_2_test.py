import json
from subprocess import CalledProcessError
import unittest
from typing import NamedTuple, Dict, List, Tuple

from unittest.mock import patch, mock_open
from unittest.mock import Mock

from gennylib.auto_tasks import Workload


class Example(NamedTuple):
    given_files: Dict[str, dict]
    given_args: List[str]
    expect_output: Tuple[str, dict]
