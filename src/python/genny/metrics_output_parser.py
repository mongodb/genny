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
        # only care about Clocks and Timers for now
        if self.section_name == 'Clocks':
            self.section_lines.append(line)
        if self.section_name == 'Timers':
            self._on_timer_line(line)

    def start_section(self, name):
        self.end()
        self.section_name = name

    def end(self):
        if self.section_name is not None:
            self.sections[self.section_name] = self.section_lines
        self.section_lines = []
        self.section_name = None

    def _system_time(self, metrics_time):
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
    out = json.dumps(timers, sort_keys=True,
        indent=4, separators=(',', ': '))
    print(out)