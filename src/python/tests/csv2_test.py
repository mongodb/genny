import os
import unittest

from genny import csv2


class CSV2Test(unittest.TestCase):

    @staticmethod
    def get_fixture(*file_path):
        return os.path.join('tests', 'fixtures', 'csv2', *file_path)

    def test_basic_parsing(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        with test_csv.data_reader() as _:
            self.assertEqual(test_csv._unix_epoch_offset_ns, 90 * (10 ** 9))
            op_map = test_csv._operation_thread_count_map
            self.assertDictEqual(op_map, {('MyActor', 'MyOperation'): 2})

    def test_data_reader(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        with test_csv.data_reader() as dr:
            self.assertEqual(next(dr),
                             ([102345.0, 0, 100, 100, 0, 1, 6, 2, 40, 2], 'MyActor',
                              'MyOperation'))

    def test_error_outcome(self):
        test_csv = csv2.CSV2(self.get_fixture('error_outcome.csv'))
        with test_csv.data_reader() as dr:
            next(dr)
            with self.assertRaisesRegex(csv2.CSV2ParsingError, 'Unexpected outcome on line'):
                next(dr)
            next(dr)

    def test_missing_clock_header(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'Unknown csv2 section title'):
            with csv2.CSV2(self.get_fixture('invalid_title.csv')).data_reader() as _:
                pass
