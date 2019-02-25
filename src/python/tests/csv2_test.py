import os
import unittest

from genny import csv2


class CSV2Test(unittest.TestCase):

    @staticmethod
    def get_fixture(*file_path):
        return os.path.join('tests', 'fixtures', 'csv2', *file_path)

    def test_basic_parsing(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        self.assertEqual(test_csv._unix_epoch_offset_ms, 90000)
        op_map = test_csv._operation_thread_count_map
        self.assertDictEqual(op_map, {('MyActor', 'MyOperation'): 2})

    def test_data_reader(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        dr = test_csv.data_reader()
        self.assertEqual(next(dr),
                         ([102345, 0, 100, 0, 1, 6, 2, 40, 2], 'MyActor', 'MyOperation'))

    def test_error_outcome(self):
        test_csv = csv2.CSV2(self.get_fixture('error_outcome.csv'))
        dr = test_csv.data_reader()
        next(dr)
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'Unexpected outcome on line'):
            next(dr)
        next(dr)

    def test_invalid_field_value(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'Error parsing CSV file'):
            csv2.CSV2(self.get_fixture('invalid_clocks.csv'))

    def test_missing_clock_header(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'BAD_CLOCK'):
            csv2.CSV2(self.get_fixture('invalid_title_clock.csv'))

    def test_missing_tc_header(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'BAD_OPERATION_THREAD_COUNT'):
            csv2.CSV2(self.get_fixture('invalid_title_thread_count.csv'))

    def test_missing_op_header(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'BAD_OPERATIONS'):
            csv2.CSV2(self.get_fixture('invalid_title_operations.csv'))

    def test_invalid_time_header(self):
        with self.assertRaisesRegex(csv2.CSV2ParsingError, 'Invalid keys for lines'):
            csv2.CSV2(self.get_fixture('wrong_order_time.csv'))
