#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Script to generate CI markdown reports from a JSON data file.
#
# This script reads the JSON file produced by ci_collect_data.py and generates
# the same three output files as the original ci_inspect_logs.py script:
#  - <results_file>: simple text file with "PASSED" or an error message
#  - <summary_file>: markdown summary report with overall results
#  - <full_report_file>: full markdown report with detailed logs
#
# Usage: ci_generate_report.py <json_file> [<results_file> <summary_file> <full_report_file>]
#  If output files are not specified, summary and full report are written to /dev/stdout
#  and results to /dev/null.

from collections import defaultdict
import json
import sys

import ci_shared
from ci_shared import (
    TestStatus, SKIP, PASS, WARNING, EXPECTED_ERROR, INVALID_EXCEPTION, ERROR, FAILURE,
    LoaderEntry, TestEntry, TestGroup,
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

def color_entry(values, full=True):
    """
    Returns a colored LaTeX-formatted string representing memory usage.
    If full is True, shows used/total as a fraction and percent; otherwise shows only percent.
    """
    if not values:
        return ""

    percent = get_percent(values)
    if full:
        return f"${{{color_cmd(percent)}\\frac{{{values[0]}}}{{{values[1]}}}\\space({percent*100:0.1f}\\\\%)}}$"
    else:
        return f"{'' if percent < 0.85 else ':warning:'} ${{{color_cmd(percent)}{percent*100:0.1f}\\\\%}}$"

def print_summary():
    """
    Prints the summary section of the report, including overall status and a recap table.
    """

    # Title
    if ci_run_passed:
        title = f"# [CI run]({JOB_LINK_STEM}#user-content-summary) PASSED :green_circle:\n"
    else:
        failed_boards = [ f"{res.status.board_icon} `{board}`" for board, res in BOARD_TESTS.items() if not res.status.passed ]
        title = f"# [CI run]({JOB_LINK_STEM}#user-content-summary) FAILED: {', '.join(failed_boards)}\n"
    f_print("<a name='summary'></a>\n")
    f_print(title)

    # Print the recap table, one line per board.
    # 8 columns:
    # - Artifact name (multi-row for boards under the same artifact)
    # - Board name
    # - Core compilation status (ok, number of warnings)
    # - Overall sketch compilation status for the core
    # - Used RAM percent
    # - Sketches tested
    # - Sketches with warnings
    # - Failed sketches (and ignored errors)
    f_print("<table>\n<tr><th>Artifact</th><th>Board</th><th>Core</th><th>Tests</th><th>RAM</th><th>Sketches</th><th>Warnings</th><th>Errors</th></tr>")

    for artifact in ARTIFACTS:
        artifact_boards = sorted(ARTIFACT_TESTS[artifact].boards)
        artifact_status = ARTIFACT_TESTS[artifact].status

        first_row_text = f"<td rowspan='{len(artifact_boards)}'>{artifact_status.board_icon} <a href='{JOB_LINK_STEM}#user-content-{artifact}'><code>{artifact}</code></a></td>"
        for board in artifact_boards:
            # Artifact name (multi-row)
            f_print(f"<tr>{first_row_text}")
            first_row_text = ""

            # Board name
            res = BOARD_LOADERS[board]
            if res.job_link:
                f_print(f"<td><code><a href='{res.job_link}'>{board}</a></code>")
            else:
                f_print(f"<td><code>{board}</code>")

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
            f_print(f"<td align='right'>\n\n{color_entry(BOARD_LOADERS[board].meminfo['RAM'], False)}\n\n</td>")

            # Test count summary
            tests_str = len(res.tests) or "-"
            warnings_str = res.counts[WARNING] or "-"
            errors_str = f"<b>{res.counts[ERROR]}</b>" if res.counts[ERROR] else "-"
            if res.counts[EXPECTED_ERROR]:
                if errors_str == "-": # only expected errors
                    errors_str = f"<i>({res.counts[EXPECTED_ERROR]}*)</i>"
                else: # both actual and expected errors
                    errors_str += f" <i>(+{res.counts[EXPECTED_ERROR]}*)</i>"
            f_print(f"<td align='right'>{tests_str}</td><td align='right'>{warnings_str}</td><td align='right'>{errors_str}</td></tr>")
    f_print("</table>\n")

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

def print_test_matrix(artifact, artifact_boards, title, sketch_filter=lambda x: True):
    """
    Prints a matrix of test results for a given artifact and its boards. The
    sketch_filter function determines which sketches to include. No table is printed
    if no sketches pass the filter.
    """

    # Build the header row, which includes board names and board statuses.
    # Headers have a link to the board's CI job, if available.
    header_row = f"<tr><th colspan='2'><code>{artifact}</code> {title}</th>"
    for board in artifact_boards:
        res = BOARD_TESTS[board]
        header_col = f"<code>{board}</code><br>{res.status.board_icon}"
        header_row += f"<th>{header_col}</th>"
    header_row += "</tr>"

    # Group sketches by library
    sketch_groups = defaultdict(list)
    for sketch in ARTIFACT_TESTS[artifact].sketches:
        res = SKETCH_TESTS[artifact][sketch]
        if not sketch_filter(res):
            continue

        for group, name in res.group_names:
            sketch_groups[group].append((name, res))

    # Build the data rows, grouping libraries together. Each row corresponds to
    # a sketch, each cell to the test result icon of that sketch on that board.
    data_rows = []
    invalid_exceptions = defaultdict(set)
    for group in sorted(sketch_groups.keys()):
        if group:
            data_rows.append(f"<tr><th colspan='2' align='left'><code>{group}</code></th><th colspan={len(artifact_boards)}><code>{group}</code></th></tr>")
            #data_rows.append(f"<tr><th colspan='2' align='left'><code>{group}</code></th>{''.join('<td align=center>---</td>' for x in artifact_boards)}</tr>")
        for sample, res in sorted(sketch_groups[group]):
            row_data = f"<tr><td>{res.status.test_icon}</td>"
            # If there are issues, make the sketch name a link to the detailed logs below
            name_link = f"<code>{sample}</code>"
            if res.tests_with_issues:
                sketch = next(iter(res.sketches)) # only one
                sketch_id = sketch.replace('/', '_').replace(' ', '_').replace('-', '_')
                name_link = f"<a href='{JOB_LINK_STEM}#user-content-{artifact}_{sketch_id}'>{name_link}</a>"
            row_data += f"<td>{name_link}</td>"

            for board in artifact_boards:
                # there are multiple tests per sketch&board due to FQBN variations
                status = max( [ t.status for t in res.tests if t.board == board ], default=TestStatus.SKIP)
                if status == INVALID_EXCEPTION:
                    invalid_exceptions[board].add(sample)
                row_data += f"<td align='center'>{status.test_icon}</td>"

            row_data += "</tr>"
            data_rows.append(row_data)

    if not data_rows:
        return

    # Print the table
    f_print("<table>")
    f_print(header_row)
    for row in data_rows:
        f_print(row)
    f_print("</table>\n")

    if invalid_exceptions:
        f_print(f"<details><summary><code>{artifact}</code> :interrobang: unnecessary <code>known_example_issues.txt</code> entries</summary>")
        f_print("<blockquote><br><table><tr><th>Board</th><th>Sketch</th></tr>")
        for board in sorted(invalid_exceptions.keys()):
            sketches = sorted(invalid_exceptions[board])
            first_row_text = f"<td rowspan={len(sketches)}><code>{board}</code></td>"
            for sketch in sketches:
                f_print(f"<tr>{first_row_text}<td><code>{sketch}</code></td></tr>")
                first_row_text = ""
        f_print("</table></blockquote></details>\n")

    # Print detailed logs for sketches with issues
    for group in sorted(sketch_groups.keys()):
        for sample, res in sorted(sketch_groups[group]):
            if not res.tests_with_issues:
                continue

            # Header and anchor for the detailed logs
            sketch = next(iter(res.sketches)) # only one
            sketch_id = sketch.replace('/', '_').replace(' ', '_').replace('-', '_')
            f_print(f"<a name='{artifact}_{sketch_id}'></a>")
            f_print(f"<details name='{artifact}_{title}'><summary><code>{artifact}</code> logs for {res.status.test_icon} <code>{group}</code> <code>{sample}</code></summary>")
            f_print("<blockquote><br><table>")

            # Test logs by board, group similar messages
            boards_by_issues = defaultdict(list)   # { issues: list of boards }
            for test in sorted(res.tests_with_issues, key=lambda x: x.status, reverse=True):
                test_text = f"<code>{test.board}:{test.link_mode}</code>"
                if test.job_link:
                    test_text += f" (<a href='{test.job_link}'>full log</a>)"
                test_text = f"{test.status.test_icon} {test_text}"

                boards_by_issues[tuple(test.issues)].append(test_text)

            for issues, board_texts in boards_by_issues.items():
                f_print(f"<tr><td>{', '.join(board_texts)}<br>\n\n```")
                for line in issues:
                    f_print(line)
                f_print("```\n</td></tr>")

            f_print("</table></blockquote></details>\n")

def print_mem_report(artifact, artifact_boards):
    """
    Prints the memory usage report for the given artifact and its boards.
    """

    # Header row, 10 columns:
    #  - Icon for high usage
    #  - Board name
    #  - SoC
    #  - FLASH usage
    #  - RAM usage
    #  - User sketch size
    #  - User heaps: SYS, LIBC
    #  - OS heaps: LLEXT, MBEDTLS
    f_print("<table><tr>", end='')
    f_print("<th rowspan='2' colspan='2'>Board</th>", end='')
    f_print("<th rowspan='2'>SoC</th>", end='')
    f_print("<th rowspan='2'>FLASH</th>", end='')
    f_print("<th rowspan='2'>RAM</th>", end='')
    f_print("<th rowspan='2'>User<br>sketch</th>", end='')
    f_print("<th colspan='2'>User heaps</th>", end='')
    f_print("<th colspan='2'>OS heaps</th>", end='')
    f_print("</tr>")
    f_print("<tr><th>SYS</th><th>LIBC</th><th>LLEXT</th><th>MBEDTLS</th></tr>")

    # Data rows
    for soc, board in sorted((ALL_BOARD_DATA[board]['soc'], board) for board in artifact_boards):
        max_pct = max([ get_percent(BOARD_LOADERS[board].meminfo[r]) for r in ('FLASH', 'RAM') ])
        icon = ':warning:' if max_pct > 0.85 else ''
        board_str = board.replace('_', '\\\\_')

        row = [
                icon,
                f"${{{color_cmd(max_pct)}\\texttt{{{board_str}}}}}$",
                f"<code>{soc}</code>",
                color_entry(BOARD_LOADERS[board].meminfo['FLASH']),
                color_entry(BOARD_LOADERS[board].meminfo['RAM']),
                f"${{{ BOARD_LOADERS[board].meminfo['SKETCH'][1] }}}$",
                f"${{{ BOARD_LOADERS[board].config.get('CONFIG_HEAP_MEM_POOL_SIZE', 0) }}}$",
                f"${{{ BOARD_LOADERS[board].config['CONFIG_SRAM_SIZE']*1024 - BOARD_LOADERS[board].meminfo['RAM'][0] }}}$",
                f"${{{ BOARD_LOADERS[board].config['CONFIG_LLEXT_HEAP_SIZE']*1024 }}}$",
                f"${{{ BOARD_LOADERS[board].config.get('CONFIG_MBEDTLS_HEAP_SIZE', '-') }}}$"
              ]

        # Print each cell with its own alignment
        f_print("<tr>")
        col_aligns = ['center', 'left', 'center', 'right', 'right', 'right', 'right', 'right', 'right', 'right']
        for index, cell in enumerate(row):
            f_print(f"<td align='{col_aligns[index]}'>\n\n{cell}\n\n</td>")
        f_print("</tr>")
    f_print("</table>")

    # Gather SoC-specific regions by SoC
    extra_data_present = False
    for soc in sorted(list(set([ ALL_BOARD_DATA[board]['soc'] for board in artifact_boards ]))):
        soc_boards = [ board for board in artifact_boards if ALL_BOARD_DATA[board]['soc'] == soc ]
        sorted_regions = sorted(r for r in REGIONS_BY_SOC[soc] if r not in ('FLASH', 'RAM', 'SKETCH'))
        if not sorted_regions:
            continue

        # this soc has extra regions to report, add details header if not done yet
        if not extra_data_present:
            f_print("<details><summary>SoC-specific data</summary><blockquote><br>\n")
            extra_data_present = True

        f_print(f"<table><tr><th rowspan='{len(soc_boards)+1}'><code>{soc}</code></th><th>Board</th>")
        for r in sorted_regions:
              f_print(f"<th>{r}</th>")
        f_print("</tr>")
        for board in sorted(soc_boards):
            f_print(f"<tr><th><code>{board}</code></th>")
            for r in sorted_regions:
                # Some regions may be missing on some boards depending on build config
                if r in BOARD_LOADERS[board].meminfo:
                    f_print(f"<td>\n\n{color_entry(BOARD_LOADERS[board].meminfo[r])}\n\n</td>")
                else:
                    f_print(f"<td></td>")
            f_print("</tr>")
        f_print("</table>\n")

    # Close extra data details if opened
    if extra_data_present:
        f_print("</blockquote></details>")

# ---------------------------------------------------------------------------
# Main Logic
# ---------------------------------------------------------------------------

if not len(sys.argv) in (2, 5):
    print("Usage: ci_generate_report.py <json_file> [<results_file> <summary_file> <full_report_file>]")
    sys.exit(1)

json_file = sys.argv[1]

if len(sys.argv) == 5:
    results_file = sys.argv[2]
    summary_file = sys.argv[3]
    full_report_file = sys.argv[4]
else:
    results_file = "/dev/null"
    summary_file = full_report_file = "/dev/stdout"

# Load data from JSON
with open(json_file, 'r') as f:
    data = json.load(f)

JOB_LINK_STEM = data['job_link_stem']
ARTIFACTS = data['artifacts']
ci_run_passed = data['ci_run_passed']
ALL_BOARD_DATA = data['all_board_data']
REGIONS_BY_SOC = {soc: set(regions) for soc, regions in data['regions_by_soc'].items()}

# Reconstruct BOARD_LOADERS from JSON
BOARD_LOADERS = {
    board: LoaderEntry.from_dict(loader)
    for board, loader in data['board_loaders'].items()
}

# Reconstruct all test entries and rebuild groups via track()
ALL_TESTS = [TestEntry.from_dict(t) for t in data['all_tests']]

ARTIFACT_TESTS = defaultdict(TestGroup)
BOARD_TESTS = defaultdict(TestGroup)
SKETCH_TESTS = defaultdict(lambda: defaultdict(TestGroup))
for t in ALL_TESTS:
    ARTIFACT_TESTS[t.artifact].track(t)
    BOARD_TESTS[t.board].track(t)
    SKETCH_TESTS[t.artifact][t.sketch].track(t)

# Update the results file
with open(results_file, 'w') as f:
    f.write('Tests PASSED.' if ci_run_passed else '::error::Tests FAILED.');
    f.write(f" See {JOB_LINK_STEM}#user-content-summary for details.\n")

# Write the summary to the summary file
with open(summary_file, 'w') as f:
    f_print = lambda *args, **kwargs: print(*args, file=f, **kwargs)

    print_summary()

# Write the detailed information to the full report file
with open(full_report_file, 'w') as f:
    f_print = lambda *args, **kwargs: print(*args, file=f, **kwargs)

    # Print the test matrix sections, per artifact
    for artifact in ARTIFACTS:
        artifact_boards = sorted([ board for board in ARTIFACT_TESTS[artifact].boards if BOARD_TESTS[board].status not in (FAILURE, SKIP) ])

        if not artifact_boards:
            continue

        f_print(f"<a name='{artifact}'></a>")
        f_print("\n---\n")

        # print loader build warnings, if any
        if any(BOARD_LOADERS[board].status != PASS for board in artifact_boards):
            summary = f"<code>{artifact}</code> loader build warnings"
            f_print(f"<details><summary>{summary}</summary><blockquote><br>\n")
            f_print("<table>")
            f_print("<tr><th>Board</th><th>Warnings</th></tr>")
            for board in artifact_boards:
                if BOARD_LOADERS[board].status == PASS:
                    continue
                f_print(f"<tr><td><code>{board}</code></td><td><pre>")
                for warning in BOARD_LOADERS[board].warnings:
                    f_print(warning)
                f_print("</pre></td></tr>")
            f_print("</table>\n")
            f_print("</blockquote></details>\n")

        # print failed tests matrix
        print_test_matrix(artifact, artifact_boards, "issues", sketch_filter=lambda res: res.status >= EXPECTED_ERROR)

        # print successful tests matrix in a collapsible section
        successful_tests = ARTIFACT_TESTS[artifact].counts[PASS] + ARTIFACT_TESTS[artifact].counts[WARNING]
        warning_tests = ARTIFACT_TESTS[artifact].counts[WARNING]
        if successful_tests:
            summary = f"{successful_tests} successful <code>{artifact}</code> tests hidden"
            if warning_tests:
                summary += f" ({warning_tests} with warnings)"

            f_print(f"<details><summary>{summary}</summary><blockquote><br>\n")
            print_test_matrix(artifact, artifact_boards, "tests", sketch_filter=lambda res: res.status.built)
            f_print("</blockquote></details>\n")

        # print memory usage report in a collapsible section
        f_print(f"<details><summary>Memory usage report for <code>{artifact}</code></summary><blockquote><br>")
        print_mem_report(artifact, artifact_boards)
        f_print("</blockquote></details>")
