import os
import unittest

from genny import csv2


class CSV2Test(unittest.TestCase):

    def get_fixture(self, *file_names):
        return os.path.join('tests', 'fixtures', *file_names)

    def test_csv2_parsing(self):
        test_csv = csv2.CSV2(self.get_fixture('csv2', 'barebones.csv'))
