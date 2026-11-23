#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script writes the test lists of the boards native_sim poses as (see
# the 'mimic' menu of native_sim in boards.txt): for each board of the core
# artifacts, the same list test-core builds for it, i.e. ci_apply_tests.sh
# run on the content of the board's core package.
#
# Each list is computed on a fresh copy of the core libraries, filtered as
# package_core.sh does for the board's artifact: the fetched repositories
# are extracted into it, and would otherwise add their sketches to the lists
# of the boards that come later. The sketches of the core libraries that are
# not in the native_sim package (native_sim.exc) cannot be built: they are
# moved to a separate list. The sketches listed in the known issues of the
# board are not tested: they fail on the real board, so they are not supported
# there, and are moved to another list. The boards are processed in parallel.
#
# Run from the root of the core, after ci_fetch_tests.sh (test-data/ must
# be present). For each board, it writes to <list dir>/<board>/:
#  - tests.txt, libraries.txt: as ci_apply_tests.sh;
#  - unsupported.txt: the sketches of tests.txt in the known issues of the
#    board, removed from it;
#  - unavailable.txt: the other sketches of tests.txt that are not available
#    on native_sim, removed from it;
#  - failed: only if the lists of the board could not be written, with the
#    log of the attempt. The other boards are not affected, and the script
#    still succeeds.
#
# Usage: ci_native_sim_lists.sh <list dir>

set -e -o pipefail

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <list dir>"
	exit 1
fi

if [ ! -d extra/artifacts/ ] || [ ! -d test-data/ ]; then
	echo "Missing prerequisites (extra/artifacts/, test-data/)."
	exit 1
fi

LIST_DIR=$(mkdir -p "$1" && realpath "$1")
TOP_DIR=$(pwd)

source extra/common.sh

# the core libraries missing from the native_sim package, as a regexp that
# matches their sketches
UNAVAILABLE_LIBS=$(artifact_exclusions native_sim | sed -n -e 's#^libraries/\([^/]*\)$#\1#p' | paste -sd'|')
UNAVAILABLE_RE="^ArduinoCore-zephyr/libraries/(${UNAVAILABLE_LIBS:-/})/"

# unsupported <variant> <tests> <unsupported>: move the sketches of the tests
# list that are in the known issues of the variant to the unsupported list
unsupported() {
	python3 - "$@" <<-'EOF'
	import sys
	sys.path.insert(0, 'extra')
	from ci_shared import load_known_issues
	variant, tests, unsupported = sys.argv[1:]
	exceptions, _ = load_known_issues(variant)
	with open(tests) as f:
	    sketches = [ line.strip() for line in f if line.strip() ]
	known = [ s for s in sketches if any(p.match(s) for p in exceptions) ]
	with open(unsupported, 'w') as f:
	    f.writelines(f"{s}\n" for s in known)
	with open(tests, 'w') as f:
	    f.writelines(f"{s}\n" for s in sketches if s not in known)
	for s in known:
	    print(f"{s}: known issue, not tested")
	EOF
}

# board_list <board> <artifact> <variant>: write the lists of a board
# (each step is checked: this runs with set -e disabled, as the left side
# of '||')
board_list() {
	local board=$1 artifact=$2 variant=$3 work tests ret=0
	work=$(mktemp -d) || return 1
	tests=$LIST_DIR/$board/tests.txt
	{
		mkdir -p "$work/ArduinoCore-zephyr" &&
		tar -cf - -X <(artifact_exclusions "$artifact") libraries | tar -xf - -C "$work/ArduinoCore-zephyr" &&
		ln -s "$TOP_DIR/extra" "$TOP_DIR/variants" "$TOP_DIR/test-data" "$work/" &&
		(cd "$work" && extra/ci_apply_tests.sh "$artifact" "$variant" "$LIST_DIR/$board") &&
		[ -f "$tests" ] &&
		unsupported "$variant" "$tests" "$LIST_DIR/$board/unsupported.txt" &&
		{ grep -E "$UNAVAILABLE_RE" "$tests" > "$LIST_DIR/$board/unavailable.txt" || [ $? -eq 1 ] ; } &&
		{ grep -vE "$UNAVAILABLE_RE" "$tests" > "$tests.new" || [ $? -eq 1 ] ; } &&
		mv "$tests.new" "$tests"
	} || ret=1
	rm -rf "$work"
	return $ret
}

# all the boards in parallel, each with its own log, shown in order
BOARDS=$(extra/get_board_details.sh | jq -r '.[] | select(.artifact != "native_sim") | "\(.board) \(.artifact) \(.variant)"')
LOGS=$(mktemp -d)
while read -r board artifact variant ; do
	{ board_list "$board" "$artifact" "$variant" || touch "$LOGS/$board.failed" ; } > "$LOGS/$board.log" 2>&1 &
	while [ "$(jobs -rp | wc -l)" -ge "$(nproc)" ] ; do
		wait -n
	done
done <<< "$BOARDS"
wait
while read -r board artifact variant ; do
	echo "::group::Test list for $board ($artifact)"
	cat "$LOGS/$board.log"
	echo "::endgroup::"
	if [ -f "$LOGS/$board.failed" ] ; then
		echo "::error::Test list for $board failed"
		mkdir -p "$LIST_DIR/$board"
		cp "$LOGS/$board.log" "$LIST_DIR/$board/failed"
	fi
done <<< "$BOARDS"
rm -rf "$LOGS"
