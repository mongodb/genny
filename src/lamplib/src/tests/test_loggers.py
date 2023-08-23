"""Tests for loggers.py"""

import unittest
from genny.loggers import StringifyAndRedact


class StringifyAndRedactTestCase(unittest.TestCase):
    """Unit Test for StringifyAndRedact processor"""

    def test_password_string(self):
        """Test password in URL string"""

        redact_processor = StringifyAndRedact()
        input_dict = {"hello": "mongodb+srv://hello:world@mongodb-dev.net"}
        expected_dict = {"hello": "mongodb+srv://hello:[REDACTED]@mongodb-dev.net"}
        self.assertDictEqual(redact_processor(None, "", input_dict), expected_dict)

    def test_empty_password(self):
        """Test empty password in URL string"""

        redact_processor = StringifyAndRedact()
        input_dict = {"hello": "mongodb://hello:@mongodb-dev.net"}
        expected_dict = {"hello": "mongodb://hello:[REDACTED]@mongodb-dev.net"}
        self.assertDictEqual(redact_processor(None, "", input_dict), expected_dict)

    def test_no_username_password(self):
        """Test no username & password in URL string"""

        redact_processor = StringifyAndRedact()
        input_dict = {"hello": "mongodb://mongodb-dev.net"}
        expected_dict = {"hello": "mongodb://mongodb-dev.net"}
        self.assertDictEqual(redact_processor(None, "", input_dict), expected_dict)

    def test_password_in_list(self):
        """Test password in list"""

        redact_processor = StringifyAndRedact()
        input_dict = {"hello": ["mongo", "--url", "mongodb://hello:world@mongodb-dev.net"]}
        expected_dict = {
            "hello": "['mongo', '--url', 'mongodb://hello:[REDACTED]@mongodb-dev.net']"
        }
        self.assertDictEqual(redact_processor(None, "", input_dict), expected_dict)

    def test_nested_password(self):
        """Test password nested in list and dict"""

        redact_processor = StringifyAndRedact()
        input_dict = {
            "url": ["mongo --url mongodb://hello:world@mongodb-dev.net"],
            "-p flag": [{"commands": "mongo -p world; mkdir -p temp"}],
            "--password flag": ["mongo --password world"],
        }
        expected_dict = {
            "url": "['mongo --url mongodb://hello:[REDACTED]@mongodb-dev.net']",
            "-p flag": "[{'commands': 'mongo -p [REDACTED]; mkdir -p temp'}]",
            "--password flag": "['mongo --password [REDACTED]']",
        }
        self.assertDictEqual(redact_processor(None, "", input_dict), expected_dict)
