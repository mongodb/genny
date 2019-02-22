import os
import unittest

from genny import csv2


class CSV2Test(unittest.TestCase):

    def get_fixture(self, *file_names):
        return os.path.join('tests', 'fixtures', *file_names)

    def test_csv2_parsing(self):
        test_csv = csv2.CSV2(self.get_fixture('csv2', 'barebones.csv'))
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
