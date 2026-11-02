#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script filters a CI job matrix, keeping only the entries whose
# prerequisites were successfully completed and that still need to run.
#
# Each entry of the input matrix has an 'id' and lists the ids of the jobs it
# depends on in its 'requires' field (see ci_get_build_info.sh). Each job
# publishes its outcome as a 'prereq-<id>' artifact containing a <id>.json
# marker file:
#
#   {
#     "id": "<id>",
#     "status": "success" | "failure",
#     "attempt": <run attempt that produced the marker>,
#     "job_url": "<URL of the job log>"
#   }
#
# The markers are collected in <ids_dir> before running this script. They are
# also used by ci_collect_data.py to link each job log in the CI summary.
#
# Markers from attempts before <min_attempt> are ignored, so that "Re-run all
# jobs" starts from scratch. Otherwise, on a re-run, entries that already
# passed are skipped, unless one of their prerequisites was completed again
# after them.
#
# With -x, successful markers in <ext_dir> are always valid, and count as
# completed before any marker in <ids_dir>; their 'attempt' is not needed.
# This is meant for jobs of another workflow run, which cannot change during
# this one.
#
# The entries to run are printed to stdout as compact JSON; with -a, entries
# that already passed are included too. A warning is emitted for each entry
# whose prerequisites are missing, and a notice for each skipped entry.
#
# Usage: ci_filter_matrix.sh [-a] [-m <min_attempt>] [-x <ext_dir>] <ids_dir> <matrix_json>

set -e

ALL=false
MIN_ATTEMPT=1
EXT_DIR=
while getopts "am:x:" opt; do
	case $opt in
		a) ALL=true ;;
		m) MIN_ATTEMPT=$OPTARG ;;
		x) EXT_DIR=$OPTARG ;;
		*) exit 1 ;;
	esac
done
shift $((OPTIND - 1))

if [ $# -ne 2 ]; then
	echo "Usage: $0 [-a] [-m <min_attempt>] [-x <ext_dir>] <ids_dir> <matrix_json>"
	exit 1
fi

IDS_DIR=$1
MATRIX=$2

# attempt of each job that completed successfully: { id: attempt }
# (external markers count as attempt 0, before any of this run)
EXT_READY=$( ( [ -n "$EXT_DIR" ] && cat "$EXT_DIR"/*.json 2>/dev/null ) | jq -cs '
	map(select(.status == "success") | { key: .id, value: 0 }) | from_entries
')
READY=$(cat "$IDS_DIR"/*.json 2>/dev/null | jq -cs --argjson min "$MIN_ATTEMPT" --argjson ext "$EXT_READY" '
	$ext + (map(select(.status == "success" and .attempt >= $min) | { key: .id, value: .attempt }) | from_entries)
')

# classify each entry as "missing", "done" or "run"
CLASSIFIED=$(jq -c --argjson ready "$READY" 'map(
	(.requires - ($ready | keys)) as $missing |
	. + { _missing: $missing, _state: (
		if ($missing | length) > 0 then "missing"
		elif $ready[.id] != null and $ready[.id] >= ([ $ready[.requires[]] ] | max // 0) then "done"
		else "run" end
	) }
)' <<< "$MATRIX")

jq -r --argjson ready "$READY" '.[]
	| if ._state == "missing" then "::warning::Skipping \(.id): missing \(._missing | join(", "))"
	  elif ._state == "done" then "::notice::Skipping \(.id): already passed in attempt \($ready[.id])"
	  else empty end
' <<< "$CLASSIFIED" >&2

jq -c --argjson all "$ALL" 'map(select(._state == "run" or ($all and ._state == "done")) | del(._missing, ._state))' <<< "$CLASSIFIED"
