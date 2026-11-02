#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Shared constants and data classes for CI log inspection scripts.

import enum
import json
import os
import re

class TestStatus(enum.IntEnum):
    """
    Test result status codes, in increasing order of severity.
    Each member carries display metadata: test_icon, board_icon, legend.
    passed is True for statuses that do not represent a build/test failure.
    built is True for statuses that represent a build that completed successfully.
    """
    def __new__(cls, value, passed, built, test_icon, board_icon, legend):
        obj = int.__new__(cls, value)
        obj._value_ = value
        obj.passed = passed
        obj.built = built
        obj.test_icon = test_icon
        obj.board_icon = board_icon
        obj.legend = legend
        return obj

    SKIP              = (-1, True,  False, ":new_moon:",      ":new_moon:",          "Test was skipped.")
    PASS              = ( 0, True,  True,  ":green_circle:",  ":white_check_mark:",  "Test passed successfully, with no warnings or errors.")
    WARNING           = ( 1, True,  True,  ":yellow_circle:", ":white_check_mark:*", "Test completed with some warnings; no errors detected.")
    EXPECTED_ERROR    = ( 2, True,  False, ":no_entry_sign:", ":heavy_check_mark:*", "Test completed with errors, but all are known/expected.")
    INVALID_EXCEPTION = ( 3, False, False, ":interrobang:",   ":interrobang:",       "Test was expected to fail but passed; exception entry is outdated.")
    ERROR             = ( 4, False, False, ":red_circle:",    ":x:",                 "Test completed with unexpected errors.")
    FAILURE           = ( 5, False, False, ":fire:",          ":fire:",              "Test run failed to complete.")

# Module-level aliases so consumers can import status constants directly
SKIP, PASS, WARNING, EXPECTED_ERROR, INVALID_EXCEPTION, ERROR, FAILURE = TestStatus

# Loader build status data structure, one per loader
class LoaderEntry:
    def __init__(self, artifact, board, variant, job_link):
        self.artifact = artifact
        self.board = board
        self.variant = variant
        self.job_link = job_link

        self.warnings = self.read_warnings() # list of warning messages
        self.config = self.read_config() # set of Kconfig settings
        self.meminfo = self.read_meminfo() # memory usage report

        if not (self.config and self.meminfo):
            self.status = FAILURE
        elif self.warnings:
            self.status = WARNING
        else:
            self.status = PASS

    def read_config(self):
        """
        Extract Kconfig settings from the loader config file.
        Format: SYMBOL=VALUE per line or # comment lines for disabled symbols.
        """
        report_file = f"zephyr-{self.variant}.config"
        try:
            with open(report_file, 'r') as f:
                configs = {}
                for line in f:
                    if line.startswith('#') or '=' not in line:
                        continue
                    sym, val = line.strip().split('=', 1)
                    if val.startswith('"'):
                        configs[sym] = val.strip('"')
                    elif val=='y':
                        configs[sym] = 1
                    else:
                        configs[sym] = eval(val)
                return configs
        except Exception as e:
            return {}

    def read_warnings(self):
        """
        Read loader compilation warnings from the warnings file.
        File may be missing in case of no warnings.
        Format: one line per warning message.
        """
        report_file = f"zephyr-{self.variant}.warnings"
        try:
            with open(report_file, 'r') as f:
                return [ line.strip() for line in f if line.strip() ]
        except Exception as e:
            return []

    def read_meminfo(self):
        """
        Extract memory usage data from the meminfo file.
        Input format: JSON array of region_name, used_bytes, total_bytes tuples
        Output format: dict of { region_name: [ used_bytes, total_bytes ] }
        A region with a total size of 0 means the file is invalid.
        """
        report_file = f"zephyr-{self.variant}.meminfo"
        try:
            with open(report_file, 'r') as f:
                report_data = json.load(f)
                meminfo = { region.replace(':',''): [used, total] for region, used, total in report_data }
                meminfo.pop('IDT_LIST', None)
                if any(total == 0 for used, total in meminfo.values()):
                    return {}
                return meminfo
        except Exception as e:
            return {}

    def to_dict(self):
        return {
            'artifact': self.artifact,
            'board': self.board,
            'variant': self.variant,
            'job_link': self.job_link,
            'warnings': self.warnings,
            'config': self.config,
            'meminfo': self.meminfo,
            'status': int(self.status),
        }

    @classmethod
    def from_dict(cls, d):
        obj = cls.__new__(cls)
        obj.artifact = d['artifact']
        obj.board = d['board']
        obj.variant = d['variant']
        obj.job_link = d['job_link']
        obj.warnings = d['warnings']
        obj.config = d['config']
        obj.meminfo = d['meminfo']
        obj.status = TestStatus(d['status'])
        return obj


# Single test data structure, one per compilation test
class TestEntry:
    def __init__(self, artifact, board, sketch, link_mode, excepted, status, issues, job_link,
                 group=None, name=None):
        self.artifact = artifact
        self.board = board
        self.sketch = sketch
        self.link_mode = link_mode
        self.excepted = excepted
        if excepted and status == ERROR:
            self.status = EXPECTED_ERROR
        elif excepted and status.built:
            self.status = INVALID_EXCEPTION
        else:
            self.status = status
        self.issues = issues
        self.job_link = job_link

        if group is not None and name is not None:
            self.group = group
            self.name = name
        else:
            match = re.search(r'(libraries|examples)/([^/]+)/(examples/|extras/)?(.*)', sketch)
            if match:
                self.group = match.group(2)
                self.name = match.group(4)
            else:
                self.group = ""
                self.name = sketch

    @classmethod
    def failure(cls, message, artifact='', board='', job_link=None):
        """
        Create an entry for a CI test run that failed to complete.
        """
        return cls(artifact, board, 'CI test', '', False, FAILURE, [ message ], job_link)

    @classmethod
    def skipped(cls, message, artifact='', board=''):
        """
        Create an entry for CI tests that were not run.
        """
        return cls(artifact, board, 'CI test', '', False, SKIP, [ message ], None)

    def to_dict(self):
        return {
            'artifact': self.artifact,
            'board': self.board,
            'sketch': self.sketch,
            'link_mode': self.link_mode,
            'excepted': self.excepted,
            'status': int(self.status),
            'issues': self.issues,
            'job_link': self.job_link,
            'group': self.group,
            'name': self.name,
        }

    @classmethod
    def from_dict(cls, d):
        return cls(
            artifact=d['artifact'],
            board=d['board'],
            sketch=d['sketch'],
            link_mode=d['link_mode'],
            excepted=d['excepted'],
            status=TestStatus(d['status']),
            issues=d['issues'],
            job_link=d['job_link'],
            group=d['group'],
            name=d['name'],
        )


# Summary data structure, used to group test results
class TestGroup:
    def __init__(self):
        # Sets to track unique board, sketch and (group,name) tuples
        self.boards = set()
        self.sketches = set()
        self.group_names = set()
        # Counts of test results by status
        self.counts = { s: 0 for s in TestStatus }
        # Overall status of the group
        self.status = SKIP
        # List of individual TestEntry objects by feature
        self.tests = []
        self.tests_with_issues = []

    def track(self, test_entry):
        """
        Update this group with a new test entry
        """
        self.tests.append(test_entry)
        if test_entry.issues:
            self.tests_with_issues.append(test_entry)

        self.counts[test_entry.status] += 1
        self.status = max(self.status, test_entry.status)
        self.boards.add(test_entry.board)
        self.sketches.add(test_entry.sketch)
        self.group_names.add((test_entry.group, test_entry.name))


def load_known_issues(variant):
    """
    Read the list of known issues for a variant from its known_example_issues.txt file.
    Returns a tuple of:
     - exceptions: list of sketch patterns whose errors are expected
     - warning_filters: list of (sketch_pattern, warning_pattern) tuples for expected warnings
    """
    exceptions = []
    warning_filters = []
    if os.path.exists(f"variants/{variant}/known_example_issues.txt"):
        with open(f"variants/{variant}/known_example_issues.txt", 'r') as f:
            for line in f:
                line = line.split('#')[0].strip()
                if not line:
                    continue
                elif '|' in line:
                    # this is a warning filter, not an exception
                    sketch_pattern, warning_pattern = line.split('|', 1)
                    sketch_pattern = re.compile(f"^(ArduinoCore-zephyr/)?{sketch_pattern}")
                    warning_pattern = re.compile(warning_pattern)
                    warning_filters.append(( sketch_pattern, warning_pattern ))
                else:
                    # this is an exception
                    exceptions.append(re.compile(f"^(ArduinoCore-zephyr/)?{line}"))
    return exceptions, warning_filters

def load_sketch_reports(report_file):
    """
    Read the sketch reports from a compile-sketches report file.
    Returns a tuple of (sketch reports, error message); the list is empty and the
    message describes the problem if the file is missing, unreadable or empty.
    """
    try:
        with open(report_file, 'r') as f:
            report_data = json.load(f)
    except FileNotFoundError:
        return [], "Report file not found."
    except Exception as e:
        return [], f"Error reading report file: {e}"

    reports = report_data.get('boards', [{}])[0].get('sketches', [])
    if not reports:
        return [], "Test report is empty, check CI log."

    return reports, None

def evaluate_sketch(report, exceptions, warning_filters):
    """
    Evaluate a single sketch report from compile-sketches against the known issues.
    Returns a tuple of (sketch, excepted, status, issues), where status does not yet
    account for the exception (TestEntry does that).
    """
    sketch = report.get('name', 'unknown_sketch')
    success = report.get('compilation_success', False)
    issues = report.get('issues', [])

    # Replace long absolute paths with '...' for brevity.
    sketch_issues = [ re.sub(r'(/.+?)((/[^/]+){3}):', r'...\2:', issue) for issue in issues ]

    # Remove known warnings, if any
    for sketch_pattern, warning_pattern in warning_filters:
        if sketch_pattern.match(sketch):
            sketch_issues = [ issue for issue in sketch_issues if not warning_pattern.search(issue) ]

    if not success:
        status = ERROR
    elif len(sketch_issues): # Implies warnings/non-critical issues
        status = WARNING
    else:
        status = PASS

    excepted = any(pattern.match(sketch) for pattern in exceptions)
    return sketch, excepted, status, sketch_issues
