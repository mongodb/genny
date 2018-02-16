#!/usr/bin/env python2
''' Read results.json and create a perf.json based on results specification '''

from __future__ import print_function
import json
import sys

import yaml

def load_files(workload_file):
    ''' Read in the workload file and the results for parsing '''

    with open(workload_file) as workload:
        work = yaml.load(workload)

    with open('results.json') as result_file:
        results = json.load(result_file)
    return (work, results)

def process_results_for_mc(filename):
    ''' Process the results and print out a table for mission control '''

    work, results = load_files(filename)

    output = {'results': []}
    for name, result_definition in work['results'].items():
        workload_keys = name.split('.')
        this_result = results
        # Descend to the proper workload
        for key in workload_keys:
            this_result = this_result[key]
        run_time = this_result['meanMicros']
        for measure in result_definition:
            name = measure['name']
            count = 0
            for node in measure['nodes']:
                count += this_result[node]['count']

                throughput = float(count * 1000 * 1000)/ run_time
                print(">>> {0} : {1:12.2f} 1".format(name, throughput))

                output['results'].append({'results': {'ops_per_sec': throughput},
                                          'name': name,
                                          'start': this_result['Date']})
                # I think I can delete the following print statements. The
                # one before here is the important one.
                print("+--------------------------------+----------+--------------+----------+------------------------------+")
                print(   "| {0:^30}".format("Test") +
                         " | {0:^8}".format("Thread") +
                         " | {0:^12}".format("Throughput") +
                         " | {0:^8}".format("Pass?") +
                         " | {0:^28}".format("Comment") +
                         " | ")
                print("|--------------------------------+----------+--------------+----------+------------------------------|")
                print(   "| {0:^30}".format(name) +
                         " | {0:^8}".format("1") +
                         " | {0:12.2f}".format(throughput) +
                         " | {0:^8}".format("true") +
                         " | {0:^28}".format("") +
                         " | ")
                print("|--------------------------------+----------+--------------+----------+------------------------------|")

    with open('perf.json', 'w') as output_file:
        json.dump(output, output_file, indent=4, separators=(',', ': '))


if __name__ == '__main__':
    process_results_for_mc(sys.argv[1])
