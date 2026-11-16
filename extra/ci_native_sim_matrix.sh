#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script computes the native_sim test jobs: one for the loader tests,
# and one for each board native_sim poses as (see ci_native_sim_lists.sh),
# that builds the test list of that board.
#
# Usage: ci_native_sim_matrix.sh <list dir>
#   print the test matrix as a compact JSON array of objects with:
#    - group: 'loader', or the board native_sim poses as;
#    - id: the job marker id of the test job;
#    - artifact, variant, arch: the core artifact, variant and architecture
#      of the board (not for 'loader').
#
# <list dir> is the one written by ci_native_sim_lists.sh: the boards with
# sketches there, available or not, get a test job, and so do the boards
# whose lists failed (their job fails). Run from the root of the core.

set -e

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <list dir>" >&2
	exit 1
fi

BOARDS=$({
	for list in "$1"/*/tests.txt ; do
		[ -s "$list" ] || [ -s "$(dirname "$list")/unavailable.txt" ] && basename "$(dirname "$list")"
	done
	for failed in "$1"/*/failed ; do
		[ -f "$failed" ] && basename "$(dirname "$failed")"
	done
} | jq -R -s -c 'split("\n") | map(select(length > 0)) | unique')

extra/get_board_details.sh | jq -c --argjson boards "$BOARDS" '
	[ { group: "loader", id: "test-native_sim-loader" } ] +
	(map(select(.board as $b | $boards | index($b)))
	 | sort_by(.board)
	 | map({ group: .board, id: "test-native_sim-\(.board)",
	         artifact: .artifact, variant: .variant, arch: .subarch }))'
