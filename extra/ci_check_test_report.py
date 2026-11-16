#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Script to check the compilation test report of a single CI test job.
#
# The report is evaluated against the known issues of the variant with the
# same rules used for the CI summary (see ci_collect_data.py), so that a test
# job can fail as soon as it finds unexpected results, without waiting for the
# whole CI run to complete.
#
# Each unexpected result is printed as a GitHub error annotation, followed by
# a summary line. The script exits with an error if any test did not pass.
#
# Usage: ci_check_test_report.py <variant> <report_file> [<board>]
#  Must be run from a directory containing variants/<variant>/. For native_sim
#  posing as a board, <board> adds the known issues of native_sim specific to
#  that board (see known_issues_files() in ci_shared.py).

import sys

from ci_shared import (
    TestEntry,
    load_known_issues, load_sketch_reports, evaluate_test,
)

def escape_data(s):
    return s.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')

def escape_property(s):
    return escape_data(s).replace(':', '%3A').replace(',', '%2C')

def check_report(variant, report_file, mimic=None):
    """
    Returns a tuple of (total_tests, list of TestEntry that did not pass).
    """
    reports, error = load_sketch_reports(report_file)
    if error:
        return 0, [ TestEntry.failure(error) ]

    exceptions, warning_filters = load_known_issues(variant, mimic)
    failed = []
    for report in reports:
        test_entry = evaluate_test(report, exceptions, warning_filters)
        if not test_entry.status.passed:
            failed.append(test_entry)

    return len(reports), failed

if len(sys.argv) not in (3, 4):
    print("Usage: ci_check_test_report.py <variant> <report_file> [<board>]")
    sys.exit(1)

total, failed = check_report(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)

for test in failed:
    message = test.status.legend
    if test.issues:
        message += f"\n{test.issues[0]}"
    print(f"::error title={escape_property(test.sketch)}::{escape_data(message)}")

if not total:
    result = "No test results available"
elif failed:
    result = f"{len(failed)} of {total} tests did not pass"
else:
    result = f"All {total} tests passed"
print(f"{result}.")

sys.exit(1 if failed else 0)
