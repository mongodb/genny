#!/usr/bin/env python

import json
"""
Genny's CSV-ish metrics output looks like the following:

    Clocks
    SystemTime,1537814143804295
    MetricsTime,12137713413174

    Counters
    12137712905436,HelloTest.0.documents,1

    Gauges
    12137712905436,HelloTest.0.connections,1

    Timers
    12137712905436,InsertTest.1.output,2235448
    12137713296139,InsertTest.1.output,278701
    12137712901949,InsertTest.0.output,2226794
    12137713293440,InsertTest.0.output,268288
    12137710737813,HelloTest.1.output,67458
    12137713066822,HelloTest.1.output,40257
    12137710753276,HelloTest.0.output,82620
    12137713053006,HelloTest.0.output,31774

There are 4 "sections": Clocks, Counters, Gauges, and Timers.
Clocks gives the correspondence between time-points measured
using the Metrics clock and the System clock. Counters, Gauges,
and Timers all record values for their respective metrics objects
(see metrics.hpp in the Genny C++ codebase).

The format for Counters, Gauges, and Timers is:

    Timestamp in metrics-clock-time
    ,
    Actor name
    .
    Actor thread number (zero-based) (optional)
    .
    Actor operation name
    ,
    Metric data-point value

For now we only record and operate on the Clocks and Timers
sections. The other data is only available in the raw CSV,
not in any rolled-up/summarized form.
"""


class ParseError(Exception):
    def __init__(self, message, file_name, line_number):
        super().__init__("[{}:{}] {}".format(file_name, line_number, message))


class ParserResults(object):
    """
    Represents the parsed Genny metrics CSV.
    """

    def __init__(self):
        self.sections = {}
        self.clock_delta = None

        self._timers = {}

        # state-tracking
        self.section_lines = []
        self.section_name = None
        self.done_timers = False

    def add_line(self, line, file_name, line_number):
        """
        Add a line within the current section. See `start_section()`.

        :param list line: already tokenized (split on commas). list of string
        :param line_number: line number from the input file (used for error diagnostics)
        :param file_name: file name of the input file (used for error diagnostics)
        :return: None
        """
        # only care about Clocks and Timers for now
        if self.section_name is None:
            raise ParseError("Unknown section", file_name, line_number)
        elif self.section_name == 'Clocks':
            self.section_lines.append(line)
        elif self.section_name == 'Timers':
            # metrics csv files are likely to be huge and we don't
            # need to store the full data-set in-memory, at least
            # not yet. Instead just stream the line to the event-style
            # method _on_timer_line().
            self._on_timer_line(line, file_name, line_number)

    def start_section(self, name):
        """
        Indicate that we're done with the current section
        (the current type of metrics objects being processed)
        and starts the indicated section.

        This automatically calls `.end()`.

        :param name: the section to be started
        """
        self.end()
        self.section_name = name

    def end(self):
        """
        Indicate that we're done processing the current section or
        the entire file. Must be called after successive invocations
        of `.add_line()`.
        """
        if self.section_name is not None:
            self.sections[self.section_name] = self.section_lines
            if self.section_name == 'Timers':
                self.done_timers = True
        self.section_lines = []
        self.section_name = None

    def _system_time(self, metrics_time, file_name, line_number):
        """
        Only valid to be called *after* the Clocks section
        has been ended.

        Metrics timestamps are recorded in a system-dependent way
        (see metrics.hpp in the genny C++ codebase). They are
        roughly correlated with wall-clock unix timestamps. We
        correlate by doing a `.now()` on both the metrics clock
        and the system clock at roughly the same time and recording
        the timestamps from each. The delta between these two gives
        a rough correspondence between any two timestamps from the
        two clocks. It is "rough" because

        1. the system clock may change or even go backward over times
        2. we don't/can't call `.now()` on both clocks at exactly
           the same time so there is a bit of (unknown) delta
           between real times and metrics times.

        All metrics timestamps are recorded in nanoseconds. Downstream
        systems may choose to drop the precision and record
        in milliseconds.

        :param metrics_time: a timestamp from the Metrics timer.
        :return: the epoch timestamp
        """
        if self.clock_delta is not None:
            return metrics_time + self.clock_delta

        if 'Clocks' not in self.sections:
            msg = "Can only call _system_time after we've seen the Clocks section. " +\
                  "We've seen sections {}".format(set(self.sections.keys()))
            raise ParseError(msg, file_name, line_number)

        clocks = {}
        for clock in self.sections['Clocks']:
            name, time = clock[0], int(clock[1])
            clocks[name] = time

        if 'SystemTime' not in clocks or 'MetricsTime' not in clocks:
            raise ParseError('Missing SystemTime or MetricsTime from Clocks', file_name,
                             line_number)

        self.clock_delta = clocks['SystemTime'] - clocks['MetricsTime']
        return self._system_time(metrics_time, file_name, line_number)

    def timers(self):
        """
        Can only be called after we've finished the Timers section.

        :return: summarized timer values. Dictionary key is
                 "Actor Name . operation" and value is a summary (dict) about
                 all the data-points for that timer.
        """
        return self._timers

    def _on_timer_line(self, timer_line, file_name, line_number):
        """
        :param timer_line: either [metrics-timestamp, Actor.Thread.Operation, DurationMicroseconds]
                           or [metrics-timestamp, Actor.Operation, DurationMicroseconds].
        :return:
        """
        when = self._system_time(int(timer_line[0]), file_name, line_number)
        full_event = timer_line[1]
        duration = int(timer_line[2])

        event_parts = full_event.split('.')
        if len(event_parts) == 2:
            # for Genny.Setup and the like: event_parts is like
            #    [Genny, Setup]
            event_name = event_parts[0] + '.' + event_parts[1]
            thread = 1
        elif len(event_parts) == 3:
            # For regular (multi-threaded) cases, event_parts is like
            #    [MyActor, 1,
            event_name = event_parts[0] + '.' + event_parts[2]
            thread = int(event_parts[1]) + 1  # zero-based indexing
        else:
            raise ParseError("Invalid event given: [{}]".format(event_parts), file_name,
                             line_number)

        # first time we've seen data for this timer
        if event_name not in self._timers:
            self._timers[event_name] = {
                'mean': 0,
                'n': 0,
                'threads': thread,
                'started': when,
                'ended': when,
            }

        event = self._timers[event_name]

        event['threads'] = max(thread, event['threads'])

        # the started/ended keys aren't super well-defined
        event['started'] = min(when, event['started'])
        event['ended'] = max(when, event['ended'])

        # compute as streaming mean
        new_mean = ((event['mean'] * event['n']) + duration) / (event['n'] + 1)
        event['n'] = event['n'] + 1
        event['mean'] = new_mean


def parse(source, file_name):
    """
    :param source: iterable source of lines to read
    :param file_name: file name (etc) read from (used for error diagnostics reporting)
    :return: a ParserResults representing the parsed data from the source.
    """
    out = ParserResults()
    line_number = 0
    for line in source:
        line_number = line_number + 1
        if line == '':
            # blank line
            pass
        items = line.strip().split(',')
        if len(items) == 1:
            # section header
            out.start_section(items[0])
        else:
            # add line to current section
            out.add_line(items, file_name, line_number)
    out.end()
    return out


def summarize():
    """
    Reads metrics CSV from stdin and prints a summary json document to stdout.

    Entry-point (see setup.py).
    :return: None
    """
    with open('/dev/stdin', 'r') as f:
        timers = parse(f, '/dev/stdin').timers()
        out = json.dumps(timers, sort_keys=True, indent=4, separators=(',', ': '))
        print(out)
