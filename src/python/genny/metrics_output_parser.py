#!/usr/bin/env python

import json


class ParserResults(object):
    def __init__(self):
        self.sections = {}
        self.clock_delta = None

        self._timers = {}

        # state-tracking
        self.section_lines = []
        self.section_name = None

    def add_line(self, line):
        """
        :param list line: already tokenized (split on commas). list of string
        :return: None
        """
        # only care about Clocks and Timers for now
        if self.section_name == 'Clocks':
            self.section_lines.append(line)
        if self.section_name == 'Timers':
            self._on_timer_line(line)

    def start_section(self, name):
        """
        Indicate that we're done with the current section
        (the current type of metrics objects being processed)).
        This automatically calls `.end()`.
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
        self.section_lines = []
        self.section_name = None

    def _system_time(self, metrics_time):
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

        clocks = {}
        for clock in self.sections['Clocks']:
            name, time = clock[0], int(clock[1])
            clocks[name] = time

        self.clock_delta = clocks['SystemTime'] - clocks['MetricsTime']
        return self._system_time(metrics_time)

    def timers(self):
        return self._timers

    def _on_timer_line(self, timer_line):
        when = self._system_time(int(timer_line[0]))
        full_event = timer_line[1]
        duration = int(timer_line[2])

        event_parts = full_event.split('.')
        if len(event_parts) < 3:
            # for Genny.Setup and the like
            event_name = event_parts[0] + '.' + event_parts[1]
            thread = 1
        else:
            event_name = event_parts[0] + '.' + event_parts[2]
            thread = int(event_parts[1]) + 1  # zero-based indexing

        # first time we've seen data for this metrics-name
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

        new_mean = ((event['mean'] * event['n']) + duration) / (event['n'] + 1)
        event['n'] = event['n'] + 1
        event['mean'] = new_mean


def parse(path):
    out = ParserResults()

    with open(path, 'r') as f:
        for line in f:
            items = line.strip().split(',')
            if len(items) == 0:
                pass
            elif len(items) == 1:
                out.start_section(items[0])
            else:
                out.add_line(items)

    out.end()

    return out


def summarize():
    timers = parse('/dev/stdin').timers()
    out = json.dumps(timers, sort_keys=True, indent=4, separators=(',', ': '))
    print(out)
