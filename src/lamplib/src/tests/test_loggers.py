"""Tests for loggers.py"""

import unittest
from genny.loggers import StringifyAndRedact


class StringifyAndRedactTestCase(unittest.TestCase):
    """Unit Test for StringifyAndRedact processor"""

    def test_full_password(self):
        """Test empty password case"""
        redactor = StringifyAndRedact()
        input_dict = {"hello": "'mongodb+srv://hello:world@mongodb-dev.net'"}
        expected_dict = {"hello": "'mongodb+srv://hello:[REDACTED]@mongodb-dev.net'"}
        self.assertDictEqual(redactor(None, "", input_dict), expected_dict)

    def test_empty_password(self):
        """Test empty password case"""
        redactor = StringifyAndRedact()
        input_dict = {"hello": "'mongodb://hello:@mongodb-dev.net'"}
        expected_dict = {"hello": "'mongodb://hello:[REDACTED]@mongodb-dev.net'"}
        self.assertDictEqual(redactor(None, "", input_dict), expected_dict)

    def test_no_username_password(self):
        """Test no username & password case"""
        redactor = StringifyAndRedact()
        input_dict = {"hello": "'mongodb://mongodb-dev.net'"}
        expected_dict = {"hello": "'mongodb://mongodb-dev.net'"}
        self.assertDictEqual(redactor(None, "", input_dict), expected_dict)
