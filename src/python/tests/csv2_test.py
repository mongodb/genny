import os
import unittest

from genny import csv2


class CSV2Test(unittest.TestCase):

    @staticmethod
    def get_fixture(*file_path):
        return os.path.join('tests', 'fixtures', 'csv2', *file_path)

    def test_basic_parsing(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        self.assertEqual(test_csv._unix_epoch_offset_ns, 990114)
        op_map = test_csv._operation_thread_count_map
        self.assertDictEqual(op_map, {('MyActor', 'MyOperation'): '2'})
        col_hdrs = test_csv._column_headers
        self.assertEqual(len(col_hdrs), 10)
        # Just check the important names
        self.assertEqual(col_hdrs[0], 'timestamp')
        self.assertEqual(col_hdrs[1], 'actor')
        self.assertEqual(col_hdrs[2], 'thread')
        self.assertEqual(col_hdrs[3], 'operation')

    def test_data_reader(self):
        test_csv = csv2.CSV2(self.get_fixture('barebones.csv'))
        dr = test_csv.data_reader()
        self.assertEqual(next(dr), [
            10007,
            'MyActor',
            0,
            'MyOperation',
            100,
            0,
            1,
            6,
            2,
            40
        ])

    def test_invalid_field_value(self):
        self.assertRaisesRegex(csv2.CSV2ParsingError, 'Error parsing CSV file',
                               csv2.CSV2, self.get_fixture('invalid_clocks.csv'))

    def test_missing_clock_header(self):
        self.assertRaisesRegex(csv2.CSV2ParsingError, 'Expected title to be "Clocks"',
                               csv2.CSV2, self.get_fixture('invalid_header_clock.csv'))

    def test_missing_tc_header(self):
        self.assertRaisesRegex(csv2.CSV2ParsingError,
                               'Expected title to be "OperationThreadCounts"',
                               csv2.CSV2, self.get_fixture('invalid_header_thread_count.csv'))

    def test_missing_op_header(self):
        self.assertRaisesRegex(csv2.CSV2ParsingError, 'Expected title to be "Operations"',
                               csv2.CSV2, self.get_fixture('invalid_header_operations.csv'))
