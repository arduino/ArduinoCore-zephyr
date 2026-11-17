#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Script to analyze CI test logs, download previously generated size reports,
# and calculate size deltas in the official JSON report format.
#
# This scripts expects the following arguments:
#  - <prev_sha>: SHA of the base report to get, used for delta calculation
#  - <reports_folder>: directory to populate with the delta reports
#
# The script performs the following actions:
# - downloads the previous report archive from AWS
# - extracts it in the output folder
# - for each JSON report file in the output folder:
#   - calculates the delta with the corresponding report in the
#     current directory (if it exists and both reports are valid)
#   - updates the output report file with the delta information.

from collections import defaultdict
import copy
import json
import os
from pathlib import Path
import requests
import re
import sys
import subprocess

def extract_url(url, file_patterns, sha, local_path):
    """
    Download and extract a tar archive from a URL, trying with truncated
    versions of the SHA until a match is found. For each SHA, the file
    patterns are tried in order; the archive is extracted according to the
    extension of the one found (.tar.zst or .tar.bz2). The archive is
    expected to contain size reports in JSON format. The extracted files will
    be placed in the specified local path. If no archive is found for the
    given SHA, a FileNotFoundError is raised.
    """

    session = requests.Session()
    os.makedirs(local_path, exist_ok=True)

    # try with the input SHA, then with truncated versions
    # until we find a match or run out of characters
    archive_path = None
    while len(sha) > 6 and not archive_path:
        for file_pattern in file_patterns:
            file = file_pattern.format(sha)
            archive_url = f"{url}/{file}"
            try:
                response = session.get(archive_url)
                if response.status_code == 404:
                    continue
                # raise other errors
                response.raise_for_status()
            except Exception as e:
                print(f"Error downloading {archive_url}: {e}")
                raise
            archive_path = Path(local_path) / file
            with open(archive_path, 'wb') as f:
                f.write(response.content)
            print(f"Downloaded {archive_url}")
            break
        else:
            sha = sha[:-1]

    if not archive_path:
        raise FileNotFoundError(f"Could not find size archive for SHA {sha}")

    # Python's tarfile only supports zstd from 3.14
    decompress = ['-I', 'unzstd'] if archive_path.name.endswith('.tar.zst') else ['-j']
    try:
        subprocess.run(['tar', *decompress, '-xf', archive_path, '-C', local_path], check=True)
        print(f"Extracted {archive_path} to {local_path}")
    except Exception as e:
        print(f"Error extracting {archive_path}: {e}")
        raise

    os.remove(archive_path)

def update_final_deltas(final, abs_max, delta):
    """
    Update the final delta values for a given entry (e.g. "flash" or "RAM for
    global variables") based on a new delta calculation from a sketch.
    """

    if not "maximum" in final:
        final["maximum"] = abs_max
        final["delta"] = {}
        for key in delta: # rel/abs
            val = delta[key]
            final["delta"][key] = {
                "minimum": val,
                "maximum": val
            }
    else:
        final["maximum"] = max(final["maximum"], abs_max)
        for key in delta: # rel/abs
            val = delta[key]
            final["delta"][key]["minimum"] = min(final["delta"][key]["minimum"], val)
            final["delta"][key]["maximum"] = max(final["delta"][key]["maximum"], val)

def range_str(delta):
    """
    Format a delta value (with "absolute" and "relative" keys) as a string for
    display in the summary table.
    """

    if not delta:
        return "N/A"

    dmin = delta['absolute']['minimum']
    dmax = delta['absolute']['maximum']
    if not dmin and not dmax:
        return "==="

    dmin_str = f"{int(dmin)}" if dmin <= 0 else f"+{int(dmin)}"
    dmax_str = f"{int(dmax)}" if dmax <= 0 else f"+{int(dmax)}"
    if dmin == dmax:
        return dmin_str
    else:
        return f"{dmin_str}..{dmax_str}"

if not len(sys.argv) == 3:
    print("Usage: ci_size_reports.py <prev_sha> <output_folder>")
    sys.exit(1)

prev_sha = sys.argv[1]
output_folder = sys.argv[2]

headers = [ "Package",  "Board",    "Options",  "Flash",    " RAM ",    "Tests",    f"vs {prev_sha}" ]
formats = [ "{{:<{}}}", "{{:<{}}}", "{{:<{}}}", "{{:^{}}}", "{{:^{}}}", "{{:>{}}}", "{{:<{}}}" ]
data_lines = []
col_widths = [ len(header) for header in headers ]

name_regexp = re.compile(r'(libraries|examples)/([^/]+)/(examples/|extras/)?(.*)')

# get the previous data to the output folder
extract_url("https://downloads.arduino.cc/cores/zephyr/size-reports",
            [ "size-reports-{}.tar.zst", "size-reports-{}.tar.bz2" ], prev_sha, output_folder)

# find every JSON file in the output folder
old_jsons = []
max_package_len = 0
max_board_len = 0
max_mode_len = 0
for dirent in os.scandir(output_folder):
    if dirent.is_file() and dirent.name.endswith(".json"):
        package, board, opts = dirent.name[:-5].split('-')[1:4]
        old_jsons.append(( package, board, opts, dirent ))

# calculate deltas and update output files
for package, board, opts, dirent in sorted(old_jsons):
    with open(dirent.path, 'r') as f:
        old_report_data = json.load(f)
    # test if the current file exists
    if not os.path.exists(dirent.name):
        # no: remove the old report to prevent confusion
        os.remove(dirent.path)
        continue
    with open(dirent.name, 'r') as f:
        new_report_data = json.load(f)

    # the output file will be a copy of the new data, plus some fields
    output_report_data = copy.deepcopy(new_report_data)
    sketch_now_missing = 0
    sketch_was_missing = 0
    updated_sketches = 0
    finals = defaultdict(dict)

    output_sketches = output_report_data['boards'][0]['sketches']

    group_lines = []

    for sketch in sorted(output_sketches, key=lambda s: s['name']):
        match = name_regexp.search(sketch['name'])
        display_name = f"{match.group(2)} {match.group(4)}" if match else sketch['name']

        sketch_ok = sketch['compilation_success']
        old_sketch = next((s for s in old_report_data['boards'][0]['sketches'] if s['name'] == sketch['name']), None)
        old_sketch_ok = old_sketch and old_sketch['compilation_success']

        if not sketch_ok:
            # compile time issue in the new data, skip
            if old_sketch_ok:
                group_lines.append(( display_name, "-- compile failed" ))
                sketch_now_missing += 1
            continue
        if not old_sketch_ok:
            # compile time issue or missing old data, skip
            group_lines.append(( display_name, "-- old data missing" ))
            sketch_was_missing += 1
            continue
        deltas = {}
        for entry in sketch['sizes']: # list, name is "flash" or "RAM for global variables"
            key = entry['name']
            old_match = next((e for e in old_sketch['sizes'] if e['name'] == entry['name']), None)
            if not old_match:
                # major group missing in old data, skip
                group_lines.append(( display_name, f"-- old group mismatch" ))
                continue
            # add "previous" data from the old report
            entry['previous'] = old_match['current']
            curr_abs = entry['current']['absolute']
            prev_abs = entry['previous']['absolute']
            if isinstance(curr_abs, str) or isinstance(prev_abs, str):
                # "N/A", ignore delta for this entry
                continue
            deltas[key] = entry['delta'] = {
                'absolute': curr_abs - prev_abs,
                'relative': ((curr_abs - prev_abs) * 100 // prev_abs) / 100 if prev_abs > 0 else "N/A"
            }
            update_final_deltas(finals[key], entry['maximum'], entry['delta'])

        # update display name in output file
        sketch['name'] = display_name

        if not deltas:
            group_lines.append(( display_name, "-- no matches" ))
        else:
            updated_sketches += 1
            ram_delta = deltas["RAM for global variables"]['absolute'] if "RAM for global variables" in deltas else "N/A"
            flash_delta = deltas["flash"]['absolute'] if "flash" in deltas else "N/A"
            group_lines.append(( display_name, f"{flash_delta:6} {ram_delta:6}" ))

    if group_lines:
        print(f"::group::{package} {board} {opts}")
        max_name_len = max(len(display_name) for display_name, _ in group_lines)
        for display_name, text in group_lines:
           print(f"{package} {board} {opts} {display_name:{max_name_len}} {text}")
        print(f"::endgroup::")

    # update the board-specific 'sizes' list with the new deltas
    output_report_data['boards'][0]['sizes'] = []
    for entry_name, final_values in finals.items():
        output_report_data['boards'][0]['sizes'].append({
            "name": entry_name,
            **final_values
        })

    # overwite old file with new data + valid deltas
    with open(dirent.path, 'w') as f:
        json.dump(output_report_data, f, indent=2)

    # generate a line for the summary table
    changes = f"{sketch_now_missing} lost" if sketch_now_missing else ""
    changes += ", " if sketch_now_missing and sketch_was_missing else ""
    changes += f"{sketch_was_missing} new" if sketch_was_missing else ""
    data_line = [ package, board, opts,
                  range_str(finals["flash"].get('delta')),
                  range_str(finals["RAM for global variables"].get('delta')),
                  updated_sketches or '-',
                  changes ]

    col_widths = [ max(len(str(data_line[i])), col_widths[i]) for i in range(len(data_line)) ]
    data_lines.append(data_line)

# format and output table
format_string = "| " + " | ".join([ formats[i].format(col_widths[i]) for i in range(len(col_widths)) ]) + " |"
spacer_string = "+-" + "-+-".join([ "-"*col_widths[i] for i in range(len(col_widths)) ]) + "-+"

print(spacer_string)
print(format_string.format(*headers))

last_package = None
for data_line in data_lines:
    if data_line[0] != last_package:
        last_package = data_line[0]
        print(spacer_string)
    else:
        data_line[0] = ""
    print(format_string.format(*data_line))

print(spacer_string)
