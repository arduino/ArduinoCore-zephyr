#!/usr/bin/env bash
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Compile sketches for native_sim, then run the ones that have expect logic
# and check their output. Exits with code 1 if any compile or check failed.
#
# Usage: run_sketches.sh [-r <report>] [-n <name root>] [-p <name prefix>]
#                        [-l <libraries dir>]...
#                        <sketch dir>...
#
#   -r  write a compile-sketches style report to this JSON file (default:
#       $SKETCH_REPORT, if set)
#   -n  directory the sketch names are relative to (default: .)
#   -p  prefix for the sketch names (default: none)
#   -l  libraries directory for arduino-cli (repeatable)
#
# The expect script of a sketch is the first found of:
#   - <sketch dir>/test.sh
#   - ../expect/<sketch id>.sh, the sketch name with '/' replaced by '_'
# It is sourced with SKETCH set to the built binary, and checks the loader
# output with the common.sh helpers. Sketches with no expect script are
# only compiled.
#
# See common.sh for the environment variables used.

set -u

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

usage() {
	sed -n -e '/^# Usage/,/^$/s/^# \?//p' "${BASH_SOURCE[0]}"
	exit 1
}

REPORT=${SKETCH_REPORT:-}
NAME_ROOT=.
NAME_PREFIX=
# the warning flags of test_core
CLI_ARGS=(--build-property "compiler.c.extra_flags=-Wno-type-limits -Wno-missing-field-initializers -Werror=return-type"
	  --build-property "compiler.cpp.extra_flags=-Wno-type-limits -Wno-missing-field-initializers -Werror=return-type")
while getopts "r:n:p:l:" opt ; do
	case $opt in
		r) REPORT=$OPTARG ;;
		n) NAME_ROOT=$OPTARG ;;
		p) NAME_PREFIX=$OPTARG ;;
		l) CLI_ARGS+=(--libraries "$(realpath "$OPTARG")") ;;
		*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -gt 0 ] || usage

# expect_script <sketch dir> <sketch name>: print the expect script path,
# if the sketch has one
expect_script() {
	local f
	for f in "$1/test.sh" "$TESTS_DIR/../expect/$(sketch_id "$2").sh" ; do
		if [ -f "$f" ] ; then
			echo "$f"
			return
		fi
	done
}

# sketch_test <sketch dir> <expect script>: build the sketch, then run its
# checks. Sets COMPILED and RUN_FAILED (empty if not run).
sketch_test() {
	local failed
	COMPILED=false
	RUN_FAILED=
	compile "$1" "${CLI_ARGS[@]}" || return
	COMPILED=true
	[ -n "$2" ] || return 0
	failed=$FAILED
	# shellcheck disable=SC1090 # expect scripts are found at runtime
	source "$2"
	RUN_FAILED=$([ "$FAILED" -eq "$failed" ] && echo false || echo true)
}

TOTAL=0
SKETCH_FAILED=0
: > "$OUT/sketches.jsonl"
for dir in "$@" ; do
	dir=$(realpath "$dir")
	name=$NAME_PREFIX$(realpath --relative-to="$NAME_ROOT" "$dir")
	expect=$(expect_script "$dir" "$name")
	failed=$FAILED

	sketch_test "$dir" "$expect" > "$OUT/test.log" 2>&1
	[ -z "${GITHUB_ACTIONS:-}" ] && echo "=== $name" || echo "::group::$name"
	cat "$OUT/test.log"
	[ -z "${GITHUB_ACTIONS:-}" ] || echo "::endgroup::"
	TOTAL=$((TOTAL + 1))
	[ "$FAILED" -eq "$failed" ] || SKETCH_FAILED=$((SKETCH_FAILED + 1))

	[ -n "$REPORT" ] || continue
	# issues: the compiler warnings, or the whole output if it failed, plus
	# the run log if a check failed
	if $COMPILED ; then
		grep 'warning:' "$OUT/compile.log" > "$OUT/issues"
	else
		cp "$OUT/compile.log" "$OUT/issues"
	fi
	[ "$RUN_FAILED" != true ] || cat "$OUT/test.log" >> "$OUT/issues"
	# (without terminal escape sequences, from the sketch output)
	sed -e 's/\x1b\[[0-9;]*[A-Za-z]//g' "$OUT/issues" | cap_log > "$OUT/issues.capped"
	jq -n --arg name "$name" --argjson compiled "$COMPILED" --arg run_failed "$RUN_FAILED" \
		--rawfile issues "$OUT/issues.capped" '{
			name: $name,
			compilation_success: $compiled,
			issues: ($issues | split("\n") | map(select(length > 0)))
		} + if $run_failed == "" then {} else { run_success: ($run_failed == "false") } end' \
		>> "$OUT/sketches.jsonl"
done

echo "$SKETCH_FAILED of $TOTAL sketches failed"
echo "$PASSED passed, $FAILED failed"

if [ -n "$REPORT" ] ; then
	jq -s --arg board "$FQBN" '{ boards: [ { board: $board, sketches: . } ] }' \
		"$OUT/sketches.jsonl" > "$REPORT"
fi

[ "$FAILED" -eq 0 ]
