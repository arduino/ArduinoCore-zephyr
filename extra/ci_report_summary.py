#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Script to generate CI markdown reports from a JSON data file.
#
# This script reads the JSON file produced by ci_collect_data.py and writes:
#  - <results_file>: simple text file with "PASSED" or an error message
#  - <summary_file>: markdown summary with the overall results, a recap table
#    of the boards and the list of the failing sketches
# The details are in the report page, see ci_report_page.py.
#
# Usage: ci_report_summary.py [--native-sim-summary <text>] [--details-url <url>] <json_file> [<results_file> <summary_file>]
#  If output files are not specified, the summary is written to /dev/stdout
#  and results to /dev/null. The native_sim summary line (see below), if given,
#  is printed after the summary title. The links of the summary and of the
#  results point to the report page at the --details-url URL, if given,
#  otherwise to the CI run.
#
# Usage: ci_report_summary.py --native-sim [--details-url <url>] <json_file> <summary_file> <step_summary_file>
#  For the native_sim tests only (collected with no other boards): write the
#  short summary of the native_sim tests, a line linking to their report page
#  with the counts of its sketches, to the summary file, and the same line
#  with the list of the failing sketches to the step summary file. Exits with
#  an error if any native_sim test failed.

import argparse
from collections import defaultdict
import json
import sys

import ci_shared
from ci_shared import (
    TestStatus, SKIP, PASS, WARNING, EXPECTED_ERROR, INVALID_EXCEPTION, ERROR, FAILURE,
    LoaderEntry, TestEntry, TestGroup,
    RAN_ICON, RUN_FAILED_ICON,
)

BASE_COLOR = 0x20               # darkest tone
DELTA_COLOR = 0xff-2*BASE_COLOR # max color change (brightest tone - darkest tone)

def get_percent(values):
    if not values:
        return 0.0
    return values[0] / values[1]

def color_cmd(percent):
    """
    Returns a LaTeX color command string based on the given percentage.
    The color hue transitions linearly from fully green (0) to fully red (1).
    """
    color_amt = int(DELTA_COLOR * percent)
    return f"\\color{{#{BASE_COLOR + color_amt:02x}{0xff - color_amt:02x}{BASE_COLOR:02x}}}"

def color_entry(values):
    """
    Returns a colored LaTeX-formatted string representing the memory usage
    percent.
    """
    if not values:
        return ""

    percent = get_percent(values)
    return f"{'' if percent < 0.85 else ':warning:'} ${{{color_cmd(percent)}{percent*100:0.1f}\\\\%}}$"

# Recap table header, 8 columns:
# - Artifact name (multi-row for boards under the same artifact)
# - Board name
# - Core compilation status (ok, number of warnings)
# - Overall sketch compilation status for the core
# - Used RAM percent
# - Sketches tested
# - Sketches with warnings
# - Failed sketches (and ignored errors)
RECAP_TABLE_HEADER = "<table>\n<tr><th>Artifact</th><th>Board</th><th>Core</th><th>Tests</th><th>RAM</th><th>Sketches</th><th>Warnings</th><th>Errors</th></tr>"

def board_label(board):
    """
    Returns the name a board is shown with: native_sim posing as a board is
    shown with the name of that board, in the native_sim summary.
    """
    return ALL_BOARD_DATA.get(board, {}).get('mimic', board)

def counts_text(res):
    """
    Returns the sketches, warnings and errors cells of a recap row.
    """
    # per sketch, not per link mode test: a sketch counts once, with its worst status
    counts = res.sketch_counts()
    tests_str = sum(counts.values()) or "-"  # all sketches, skipped ones included
    warnings_str = counts[WARNING] or "-"
    errors_str = f"<b>{counts[ERROR]}</b>" if counts[ERROR] else "-"
    if counts[EXPECTED_ERROR]:
        if errors_str == "-": # only expected errors
            errors_str = f"<i>({counts[EXPECTED_ERROR]}*)</i>"
        else: # both actual and expected errors
            errors_str += f" <i>(+{counts[EXPECTED_ERROR]}*)</i>"
    return f"<td align='right'>{tests_str}</td><td align='right'>{warnings_str}</td><td align='right'>{errors_str}</td>"

def details_link():
    """
    Link to the details: the report page if given, otherwise the CI run.
    """
    return details_url or JOB_LINK_STEM

def print_recap_rows(artifact):
    """
    Prints the recap table rows of the boards of an artifact.
    """
    artifact_boards = sorted(ARTIFACT_TESTS[artifact].boards)
    artifact_status = max(BOARD_TESTS[board].status for board in artifact_boards)

    artifact_text = f"<a href='{details_link()}'><code>{artifact}</code></a>"
    first_row_text = f"<td rowspan='{len(artifact_boards)}'>{artifact_status.board_icon} {artifact_text}</td>"
    for board in artifact_boards:
        # Artifact name (multi-row)
        f_print(f"<tr>{first_row_text}")
        first_row_text = ""

        # Board name
        res = BOARD_LOADERS[board]
        if res.job_link:
            f_print(f"<td><code><a href='{res.job_link}'>{board_label(board)}</a></code>")
        else:
            f_print(f"<td><code>{board_label(board)}</code>")

        # Core build status + message on failure
        if res.status == FAILURE:
            f_print(f"<td align='center'>{res.status.board_icon}</td><td colspan='6'>Core build failed for this variant!</td></tr>")
            continue

        pin = f"{len(res.warnings)} :label:" if res.status == WARNING else ":green_book:"
        f_print(f"</td><td align='center'>{pin}</td>")

        # Sketch build status + message on failure or if tests were skipped
        res = BOARD_TESTS[board]
        f_print(f"<td align='center'>{res.status.board_icon}</td>")
        if res.status in (FAILURE, SKIP):
            f_print(f"<td colspan='5'>")
            f_print("<br>".join(test.issues[0] + (f" (<a href='{test.job_link}'>full log</a>)" if test.job_link else "") for test in res.tests if test.status == res.status and test.issues))
            f_print("</td></tr>")
            continue

        # Memory usage
        f_print(f"<td align='right'>\n\n{color_entry(BOARD_LOADERS[board].meminfo.get('RAM'))}\n\n</td>")

        # Test count summary
        f_print(f"{counts_text(res)}</tr>")

def print_summary():
    """
    Prints the summary section of the report, including overall status and a recap table.
    """

    # Title
    if ci_run_passed:
        title = f"# [CI run]({details_link()}) PASSED :green_circle:\n"
    else:
        failed_boards = [ f"{res.status.board_icon} `{board}`" for board, res in BOARD_TESTS.items() if not res.status.passed ]
        title = f"# [CI run]({details_link()}) FAILED: {', '.join(failed_boards)}\n"
    f_print(title)
    if native_sim_summary:
        f_print(f"{native_sim_summary}\n")

    # Print the recap table, one line per board
    f_print(RECAP_TABLE_HEADER)
    for artifact in ARTIFACTS:
        print_recap_rows(artifact)
    f_print("</table>\n")

    print_failing_sketches()

    # Print the legend
    f_print("<details><summary>Legend</summary>")
    f_print("<blockquote><br><table><tr><th align='center'>Board</th><th align='center'>Test</th><th>Status description</th></tr>")
    for status in reversed(TestStatus): # worst errors first
        f_print(f"<tr><td align='center'>{status.board_icon}</td>")
        f_print(f"<td align='center'>{status.test_icon}</td>")
        f_print(f"<td>{status.legend}</td></tr>")
    f_print("</table></blockquote></details>\n")

    # Print artifact error warnings
    for artifact in ARTIFACTS:
        artifact_boards = sorted(ARTIFACT_TESTS[artifact].boards)
        failed_boards = [ f"`{board}`" for board in artifact_boards if not BOARD_TESTS[board].status.passed ]

        if failed_boards:
            f_print("> [!CAUTION]")
            f_print(f"> `{artifact}` is blocked due to failures on {', '.join(failed_boards)}!\n")

def print_failing_sketches():
    """
    Prints the list of the failing sketches (errors and invalid exceptions:
    not the known failures, nor the CI failures shown in the recap), each
    with the boards where it fails, linked to their test job.
    """
    failing = defaultdict(dict)
    for test in sorted(ALL_TESTS, key=lambda t: (t.group, t.name, board_label(t.board))):
        if test.status in (INVALID_EXCEPTION, ERROR):
            failing[f"{test.group}/{test.name}" if test.group else test.name].setdefault(test.board, test)
    if not failing:
        return
    f_print(f"<b>Failing sketches</b> ({len(failing)})\n")
    for sketch, tests in failing.items():
        boards = ", ".join(f"{t.status.test_icon} [`{board_label(b)}`]({t.job_link})" if t.job_link
                           else f"{t.status.test_icon} `{board_label(b)}`" for b, t in tests.items())
        f_print(f"- <code>{sketch}</code>: {boards}")
    f_print("")

# native_sim sketch categories, in display order, with their icon and text
NATIVE_SIM_CATEGORIES = {
    'incomplete': (FAILURE.test_icon, "CI failure"),
    'build_failed': (ERROR.test_icon, "build failed"),
    'only_built': (PASS.test_icon, "compiled only"),
    'ran': (RAN_ICON, "passed sim"),
    'ran_failing': (RUN_FAILED_ICON, "failed sim"),
    'known_issue': (EXPECTED_ERROR.test_icon, "known issues"),
    'not_sim_ready': (SKIP.test_icon, "require libs not in native_sim"),
}

def native_sim_category(test):
    """
    Returns the category of a native_sim test (see NATIVE_SIM_CATEGORIES).
    """
    if test.status == FAILURE:
        return 'incomplete'
    if test.status == SKIP:
        return 'not_sim_ready'
    if test.status in (EXPECTED_ERROR, ERROR):
        if test.run_success is False:
            return 'ran_failing'
        return 'known_issue' if test.status == EXPECTED_ERROR else 'build_failed'
    return 'only_built' if test.run_success is None else 'ran'

def native_sim_title(name):
    """
    Returns the title line of the native_sim tests: the number of sketches
    and the given name in bold, then their counts in each category (and the
    incomplete test jobs).
    """
    totals = defaultdict(int)
    for test in ALL_TESTS:
        totals[native_sim_category(test)] += 1
    parts = { cat: f"{icon} {totals[cat]} {text}" for cat, (icon, text) in NATIVE_SIM_CATEGORIES.items() if totals[cat] }
    counts = ", ".join(part for cat, part in parts.items() if cat != 'not_sim_ready')
    if 'not_sim_ready' in parts:
        counts += "; " + parts['not_sim_ready']
    sketches = sum(totals.values()) - totals['incomplete']
    return f"<b>{sketches} {name}</b>: {counts}"

def print_native_sim_summary():
    """
    Prints the short summary of the native_sim tests: their title line,
    linking to their report page (or the CI run).
    """
    f_print(native_sim_title(f"<a href='{details_link()}'>native_sim tests</a>"))

parser = argparse.ArgumentParser(description="Write the CI summary of the collected data")
parser.add_argument("--native-sim-summary", type=str.strip,
                    help="short summary of the native_sim tests, printed after the title")
parser.add_argument("--details-url", type=str.strip,
                    help="URL of the report page, the target of the links")
parser.add_argument("--native-sim", action="store_true",
                    help="summary of the native_sim tests only (see test_native_sim.yml)")
parser.add_argument("json_file", help="data collected by ci_collect_data.py")
parser.add_argument("files", nargs="*", metavar="file",
                    help="<results_file> <summary_file>, or with --native-sim <summary_file> <step_summary_file>")
args = parser.parse_args()
if len(args.files) not in ((2,) if args.native_sim else (0, 2)):
    parser.error("wrong number of output files")

native_sim_summary = args.native_sim_summary
details_url = args.details_url
native_sim_mode = args.native_sim
json_file = args.json_file
if native_sim_mode:
    summary_file, step_summary_file = args.files
elif args.files:
    results_file, summary_file = args.files
else:
    results_file = "/dev/null"
    summary_file = "/dev/stdout"

# Load data from JSON
with open(json_file, 'r') as f:
    data = json.load(f)

JOB_LINK_STEM = data['job_link_stem']
ARTIFACTS = data['artifacts']
ci_run_passed = data['ci_run_passed']
ALL_BOARD_DATA = data['all_board_data']

# Reconstruct BOARD_LOADERS from JSON
BOARD_LOADERS = {
    board: LoaderEntry.from_dict(loader)
    for board, loader in data['board_loaders'].items()
}

# Reconstruct all test entries and rebuild groups via track()
ALL_TESTS = [TestEntry.from_dict(t) for t in data['all_tests']]

ARTIFACT_TESTS = defaultdict(TestGroup)
BOARD_TESTS = defaultdict(TestGroup)
for t in ALL_TESTS:
    ARTIFACT_TESTS[t.artifact].track(t)
    BOARD_TESTS[t.board].track(t)

if native_sim_mode:
    native_sim_passed = ci_run_passed and bool(ALL_TESTS)
    with open(summary_file, 'w') as f:
        f_print = lambda *args, **kwargs: print(*args, file=f, **kwargs)
        print_native_sim_summary()
    with open(step_summary_file, 'w') as f:
        f_print = lambda *args, **kwargs: print(*args, file=f, **kwargs)
        print_native_sim_summary()
        f_print("")
        print_failing_sketches()
    sys.exit(0 if native_sim_passed else 1)

# Update the results file
with open(results_file, 'w') as f:
    f.write('Tests PASSED.' if ci_run_passed else '::error::Tests FAILED.');
    f.write(f" See {details_link()} for details.\n")

# Write the summary to the summary file
with open(summary_file, 'w') as f:
    f_print = lambda *args, **kwargs: print(*args, file=f, **kwargs)

    print_summary()
