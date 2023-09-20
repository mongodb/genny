# /bin/python
# You may need to pip install some of the imports below if you don't have them already

import argparse
import itertools
import json
import math
import numpy as np
import os
import re
import statistics
import subprocess
import sys
import threading
import time

from tqdm import tqdm

default_metrics_path = 'build/WorkloadOutput/CedarMetrics'
default_metrics = ['throughput', 'timers.dur']


def path_to_string(path_component_array):
    return os.path.sep.join(path_component_array)


def find_ftdc_files(metrics_path):
    ls_results = [path_to_string(metrics_path + [file])
                  for file in os.listdir(path_to_string(metrics_path))]
    return [path_str
            for path_str in ls_results if os.path.isfile(path_str) and path_str.endswith(".ftdc")]


def parse_args():
    parser = argparse.ArgumentParser(
        prog='test_result_summary.py',
        description='''
This script analyzes the genny perf results and prints some summary information to the console.

Without this script, you need to use evergreen or a tool like t2 to see what happened. Those tools
are probably superior for detailed analysis, but this is meant to give quick feedback of whether
things are working as expected during development or local testing''',
        epilog='''
Please feel free to update this script to handle your metris or to adjust the output to something
you consider more useful.''')

    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument(
        '-d',
        '--workloadOutputDir',
        default=default_metrics_path,
        help="""The directory to look for the .ftdc workload output files.
                Defaults to """ + default_metrics_path)
    parser.add_argument(
        '-m',
        '--metrics',
        nargs="+",
        default=default_metrics,
        help="""Which metrics should this script look at and summarize?
             Specify as a space separated list. Defaults to """ + json.dumps(default_metrics))
    parser.add_argument(
        '--hideHistograms',
        action='store_true',
        help="Hide histogram output")
    parser.add_argument(
        '-b',
        '--nHistogramBuckets',
        type=int,
        default=15,
        help="How many buckets should we use to print the histograms? Default 15.")
    parser.add_argument(
        '-a',
        '--actorRegex',
        help="""An optional regex to apply to filter out processing results for certain actors.
        The results for ExampleActor will be stored in something like
        '.../WorkloadOutput/CedarMetrics/ExampleActor.ftdc. Your regex should assume it is working
        with just the actor name: "ExampleActor" in this case.""")
    return parser.parse_args()


def replace_suffix(string, suffixTarget, newSuffix):
    assert string.endswith(suffixTarget)
    suffix_start = string.rfind(suffixTarget)
    return string[0:suffix_start] + newSuffix

def animate(interrupt_event):
    for c in itertools.cycle(['|', '/', '-', '\\']):
        sys.stdout.write('\rprocessing ftdc file... ' + c)
        sys.stdout.flush()
        if interrupt_event.wait(0.1):
            sys.stdout.write('\rDone!     ')
            return


def convert_to_json(args, actor_file):
    tmp_file_location = replace_suffix(actor_file, ".ftdc", ".json")
    if (os.path.exists(tmp_file_location)):
        if args.verbose:
            print(
                """Found pre-existing %s. Assuming this is the result of converting
                %s to csv. Proceeding by using it rather than re-converting. Rename
                it or delete it if you don't want to use it.""" %
                (tmp_file_location, actor_file))
        return tmp_file_location

    if args.verbose:
        print("Outputing ftdc to json...")

    interrupt_event = threading.Event()
    loading_bar_thread = threading.Thread(target=animate, args=(interrupt_event,), daemon=True)
    loading_bar_thread.start()

    sub_result = subprocess.run(
        ["./build/curator/curator", "ftdc", "export", "json", "--input", actor_file, "--flattened", "--output", tmp_file_location])
    interrupt_event.set()
    assert sub_result.returncode == 0
    return tmp_file_location


def summarize_diffed_data(args, actor_name, metrics_of_interest):
    """
    Computes statistical measures on data points that have been pre-processed by diffing one
    recording from the previous recording.

    Returns a mapping from metric name to a dictionary of summary statistics, e.g. 
    {
        'timers.dur (measured in nanoseconds, displayed in milliseconds)': {
            count: 10,
            average: 43.1,
            ...
        },
        'second metric example': {...},
        ...
    }
    """
    results = {}
    for (metric_name, diffed_readings) in metrics_of_interest.items():
        if diffed_readings == []:
            print(
                "No measurements to summarize for %s metric for %s. Skipping" % (metric_name, actor_name))
            continue

        sorted_res = sorted(diffed_readings)
        if is_measured_in_nanoseconds(metric_name):
            metric_name += " (measured in nanoseconds, displayed in milliseconds)"

        def rnd(number):
            """Round a number to 4 significant digits, without going to scientific notation."""
            return np.format_float_positional(
                number, precision=4, unique=False, fractional=False, trim='k')

        try:
            mode = rnd(statistics.mode(diffed_readings))
        except statistics.StatisticsError:
            mode = "N/A - multiple modes found. Try upgrading to python 3.8 which will return the first"

        results[metric_name] = {
            "count": len(diffed_readings),
            "average": rnd(statistics.mean(diffed_readings)),
            "median": rnd(statistics.median_grouped(diffed_readings)),
            "mode": mode,
            "stddev": rnd(statistics.stdev(diffed_readings)) if len(diffed_readings) > 1 else None,
            "[min, max]": [rnd(sorted_res[0]), rnd(sorted_res[-1])],
            "sorted_raw_data": sorted_res
        }
        if args.verbose:
            print("Summarized", metric_name)
            pretty_print_summary(args, results[metric_name], "\t")
    return results


def summarize_readings(args, actor_name, metrics_of_interest, first_line, last_line, n_rows):
    results = summarize_diffed_data(args, actor_name, metrics_of_interest)

    if args.verbose:
        print(
            "Finished summarizing diffed data, computing metrics from the the last row...")

    if first_line is None:
        if args.verbose:
            print("No first line, assuming this means there was no data.")
        return results

    if "throughput" in args.metrics:
        if "counters.ops" in last_line and "timers.dur" in last_line:
            n_ops = float(last_line["counters.ops"])
            elapsed_seconds = (last_line["ts"] - first_line["ts"]) / 1000
            if n_rows == 1:
                # If there is only a single recording, then assume that its
                # a synthetic recording and use that as the absolute elapsed
                # time.
                #
                # NOTE: `timers.dur` is expected to be in nanoseconds.
                elapsed_seconds = last_line["timers.dur"] / 1e9

            results["throughput"] = {
                "ops": n_ops,
                "seconds": elapsed_seconds,
                "ops per second": round(float('inf') if elapsed_seconds == 0 else n_ops / elapsed_seconds, 4),
            }
            if args.verbose:
                print("throughput:")
                pretty_print_summary(args, results["throughput"], "\t")

    if "errors" in args.metrics:
        if "counters.errors" in last_line:
            n_errors = last_line["counters.errors"]
            if n_errors != 0:
                print("WARNING non-zero error count: ", n_errors)
                results["errors"] = {"total": n_errors}
            elif args.verbose:
                print("errors: ", n_errors)

    return results


def is_measured_in_nanoseconds(metric_name):
    return metric_name.startswith("timers.")


def process_json(args, actor_name, json_reader):
    if args.verbose:
        print("Analyzing the following metrics", args.metrics)

    # These we can calculate by just looking at the last row to get the totals.
    metrics_handled_later = ["throughput", "errors"]

    for metric_name in args.metrics:
        if metric_name in metrics_handled_later:
            continue

        # Other metrics should be easy to add, but are untested so not included here.
        assert metric_name.endswith(".total") or metric_name.endswith('.dur'), """
        Unexpected metric that I don't know how to summarize: %s. If you know how you'd like
        to summarize this, please add the analysis to this python script!""" % metric_name

    # Store (last reading, all readings) for each metric.
    metrics_of_interest = {
        m: (0,  []) for m in args.metrics if m not in metrics_handled_later}

    # Now scan the '.json' data, row by row.
    # The metrics we know how to process so far are all stored row-by-row with a running total. So
    # if we're interested in say the average latency for an operation, we'll calculate the latency
    # for _each_ operation by subtracting each reading's previous recording to get the diff
    # associated for just that reading.
    n_rows = 0
    first_line = None
    last_line = None
    for raw_recording in tqdm(json_reader, desc="Processing expanded JSON results"):
        n_rows += 1
        recording = None
        try:
            recording = json.loads(raw_recording)
        except json.JSONDecodeError:
            print(f"Found invalid json: {raw_recording}")
            raise
        last_line = recording
        if n_rows == 1:
            first_line = recording
        for metric_name in metrics_of_interest:
            if metric_name not in recording:
                print("Unable to find metric with the name '%s'. Available metrics: %s" % (
                    metric_name, json.dumps(recording)))
                print(f"Skipping actor analysis: {actor_name}")
                return {metric_name: []}

            this_reading = recording[metric_name]

            # Convert nanoseconds to milliseconds - nanos have too many digits for humans to easily
            # interpret.
            if is_measured_in_nanoseconds(metric_name):
                this_reading /= 1000.0 * 1000.0

            (last_reading, all_readings) = metrics_of_interest[metric_name]
            all_readings.append(this_reading - last_reading)
            metrics_of_interest[metric_name] = (this_reading, all_readings)

    if args.verbose:
        print("Finished reading %d rows. Now processing data for output" %
              n_rows)

    # strip out the 'last recording' piece of information - not needed anymore.
    just_data = {m: all_data for (m, (_, all_data))
                 in metrics_of_interest.items()}
    return summarize_readings(args, actor_name, just_data, first_line, last_line, n_rows)


def print_histogram_bucket(prefix, global_max, bucket_min, bucket_max, end_bracket, stars, n_items):
    # Compute max digits to limit the width of our bucket display. We want the bucket boundaries to
    # be aligned, but we don't want something with lots of extra whitespace, like:
    # [0       ,10      ): ****
    # [10      ,12      ): **
    # ....
    max_digits = math.ceil(math.log(global_max, 10))
    bound_fmt = "%" + str(max_digits) + "d"
    fmt_string = "%s[" + bound_fmt + "," + bound_fmt + "%s: %s\t(%d)"
    print(fmt_string %
          (
              prefix,
              round(bucket_min),
              round(bucket_max),
              end_bracket,
              stars,
              n_items
          ))


def print_histogram(data_points, n_buckets, prefix=""):
    n_buckets = min(len(data_points), n_buckets)
    # 'processed' must be sorted.
    min_v, max_v = data_points[0], data_points[-1]
    step = (max_v - min_v) / n_buckets
    split_points = [min_v + step *
                    i for i in range(n_buckets)] + [data_points[-1]]

    data_idx = 0
    # One more split point than we have buckets. e.g. 11 splits means 10 buckets.
    for splits_idx in range(1, len(split_points)):
        bucket_min = split_points[splits_idx - 1]
        bucket_max = split_points[splits_idx]

        starting_idx = data_idx
        while data_idx < len(data_points) and data_points[data_idx] < split_points[splits_idx]:
            data_idx += 1
        n_items = data_idx - starting_idx

        if splits_idx == len(split_points) - 1:
            # Everything left has to go in this bucket
            n_items += (len(data_points) - data_idx)

        # Some workloads record thousands or more readings - don't want to print that many stars.
        max_stars = 60
        stars = "*"*max_stars + "..." if (n_items > max_stars) else "*"*n_items

        print_histogram_bucket(
            prefix,
            max_v,
            bucket_min,
            bucket_max,
            ")" if splits_idx != len(split_points) - 1 else "]",
            stars,
            n_items)


def pretty_print_summary(args, summary, prefix=""):
    for key in summary:
        if key == "sorted_raw_data":
            continue

        if summary[key] is not None:
            print("%s%-10s: %-8s" % (prefix, key, summary[key]))

    if not args.hideHistograms and "sorted_raw_data" in summary:
        print("%shistogram:" % prefix)
        print_histogram(summary["sorted_raw_data"],
                        args.nHistogramBuckets, prefix + "\t")


def extract_actor_name(actor_file):
    return actor_file[actor_file.rfind('/') + 1:actor_file.rfind('.')]


def parse_actor_regex(args):
    actor_regex = re.compile('.*')  # Match all actors by default.
    if args.actorRegex is not None:
        try:
            actor_regex = re.compile(args.actorRegex)
        except:
            print("Unable to translate provided regex: ", args.actorRegex)
            raise

    return actor_regex


def main():
    args = parse_args()

    metrics_path = args.workloadOutputDir.split(os.path.sep)
    result_ftdc_files = find_ftdc_files(metrics_path)

    if result_ftdc_files == []:
        raise AssertionError("No results found to summarize")

    if args.verbose:
        print("Found", len(result_ftdc_files), "results to analyze.")

    actor_regex = parse_actor_regex(args)

    global_summaries = {}
    for actor_file in result_ftdc_files:
        actor_name = extract_actor_name(actor_file)
        if actor_regex.match(actor_name) is None:
            if args.verbose:
                print("Skipping actor %s since it doesn't match regex" %
                      actor_name)
            continue

        if args.verbose:
            print("Analyzing", actor_name, "...")

        global_summaries[actor_name] = {}

        tmp_file = convert_to_json(args, actor_file)
        with open(tmp_file, 'r') as json_reader:
            metric_summaries = process_json(args, actor_name, json_reader)

            for (metric_name, summary_data) in metric_summaries.items():
                global_summaries[actor_name][metric_name] = summary_data

    for (actor_name, metrics) in global_summaries.items():
        print(actor_name, "summary:")
        for (metric_name, summary_data) in metrics.items():
            print("\t%s:" % metric_name)
            pretty_print_summary(args, summary_data, "\t\t")
        print("\n")


if __name__ == "__main__":
    main()
