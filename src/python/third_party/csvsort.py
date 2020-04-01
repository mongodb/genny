# -*- coding: utf-8 -*-

"""
This file is originally from the csvsort project:
https://bitbucket.org/richardpenman/csvsort

MongoDB Modifications:
1. add the quoting=quoting argument to csv.reader()
"""

import csv
import heapq
import os
import sys
import tempfile
from optparse import OptionParser

csv.field_size_limit(sys.maxsize)


class CsvSortError(Exception):
    pass


def csvsort(
    input_filename,
    columns,
    output_filename=None,
    max_size=100,
    has_header=True,
    delimiter=",",
    show_progress=False,
    quoting=csv.QUOTE_MINIMAL,
):
    """Sort the CSV file on disk rather than in memory.

    The merge sort algorithm is used to break the file into smaller sub files

    Args:
        input_filename: the CSV filename to sort.
        columns: a list of columns to sort on (can be 0 based indices or header
            keys).
        output_filename: optional filename for sorted file. If not given then
            input file will be overriden.
        max_size: the maximum size (in MB) of CSV file to load in memory at
            once.
        has_header: whether the CSV contains a header to keep separated from
            sorting.
        delimiter: character used to separate fields, default ','.
        show_progress (Boolean): A flag whether or not to show progress.
            The default is False, which does not print any merge information.
        quoting: How much quoting is needed in the final CSV file.  Default is
            csv.QUOTE_MINIMAL.
    """

    with open(input_filename) as input_fp:
        reader = csv.reader(input_fp, delimiter=delimiter, quoting=quoting)
        if has_header:
            header = next(reader)
        else:
            header = None

        columns = parse_columns(columns, header)

        filenames = csvsplit(reader, max_size, quoting)
        if show_progress:
            print("Merging %d splits" % len(filenames))
        for filename in filenames:
            memorysort(filename, columns, quoting)
        sorted_filename = mergesort(filenames, columns, quoting)

    # XXX make more efficient by passing quoting, delimiter, and moving result
    # generate the final output file
    with open(output_filename or input_filename, "w") as output_fp:
        writer = csv.writer(output_fp, delimiter=delimiter, quoting=quoting)
        if header:
            writer.writerow(header)
        with open(sorted_filename) as sorted_fp:
            for row in csv.reader(sorted_fp, quoting=quoting):
                writer.writerow(row)

    os.remove(sorted_filename)


def parse_columns(columns, header):
    """check the provided column headers
    """
    for i, column in enumerate(columns):
        if isinstance(column, int):
            if header:
                if column >= len(header):
                    raise CsvSortError('Column index is out of range: "{}"'.format(column))
        else:
            # find index of column from header
            if header is None:
                raise CsvSortError(
                    "CSV needs a header to find index of this column name:" + ' "{}"'.format(column)
                )
            else:
                if column in header:
                    columns[i] = header.index(column)
                else:
                    raise CsvSortError('Column name is not in header: "{}"'.format(column))
    return columns


def csvsplit(reader, max_size, quoting):
    """Split into smaller CSV files of maximum size and return the filenames.
    """
    max_size = max_size * 1024 * 1024  # convert to bytes
    writer = None
    current_size = 0
    split_filenames = []

    # break CSV file into smaller merge files
    for row in reader:
        if writer is None:
            ntf = tempfile.NamedTemporaryFile(delete=False, mode="w")
            writer = csv.writer(ntf, quoting=quoting)
            split_filenames.append(ntf.name)

        writer.writerow(row)
        current_size += sys.getsizeof(row)
        if current_size > max_size:
            writer = None
            current_size = 0
    return split_filenames


def memorysort(filename, columns, quoting):
    """Sort this CSV file in memory on the given columns
    """
    with open(filename) as input_fp:
        rows = [row for row in csv.reader(input_fp, quoting=quoting)]
    rows.sort(key=lambda row: get_key(row, columns))
    with open(filename, "w") as output_fp:
        writer = csv.writer(output_fp, quoting=quoting)
        for row in rows:
            writer.writerow(row)


def get_key(row, columns):
    """Get sort key for this row
    """
    return [row[column] for column in columns]


def decorated_csv(filename, columns, quoting):
    """Iterator to sort CSV rows
    """
    with open(filename) as fp:
        for row in csv.reader(fp, quoting=quoting):
            yield get_key(row, columns), row


def mergesort(sorted_filenames, columns, quoting, nway=100):
    """Merge these 2 sorted csv files into a single output file

    `nway` defaults to 100 to allow for 100MB * 100 = 10GB of files
    to be read in at once with the default file size of 100MB.
    """
    merge_n = 0
    while len(sorted_filenames) > 1:
        merge_filenames, sorted_filenames = sorted_filenames[:nway], sorted_filenames[nway:]

        with tempfile.NamedTemporaryFile(delete=False, mode="w") as output_fp:
            writer = csv.writer(output_fp, quoting=quoting)
            merge_n += 1
            for _, row in heapq.merge(
                *[decorated_csv(filename, columns, quoting) for filename in merge_filenames]
            ):
                writer.writerow(row)

            sorted_filenames.append(output_fp.name)

        for filename in merge_filenames:
            os.remove(filename)
    return sorted_filenames[0]


def main():
    parser = OptionParser()
    parser.add_option(
        "-c", "--column", dest="columns", action="append", help="column of CSV to sort on"
    )
    parser.add_option(
        "-s",
        "--size",
        dest="max_size",
        type="float",
        default=100,
        help="maximum size of each split CSV file in MB (default 100)",
    )
    parser.add_option(
        "-n",
        "--no-header",
        dest="has_header",
        action="store_false",
        default=True,
        help="set CSV file has no header",
    )
    parser.add_option("-d", "--delimiter", default=",", help='set CSV delimiter (default ",")')
    args, input_files = parser.parse_args()

    if not input_files:
        parser.error("What CSV file should be sorted?")
    elif not args.columns:
        parser.error("Which columns should be sorted on?")
    else:
        # escape backslashes
        args.delimiter = args.delimiter.decode("string_escape")
        args.columns = [int(column) if column.isdigit() else column for column in args.columns]
        csvsort(
            input_files[0],
            columns=args.columns,
            max_size=args.max_size,
            has_header=args.has_header,
            delimiter=args.delimiter,
        )


if __name__ == "__main__":
    main()
