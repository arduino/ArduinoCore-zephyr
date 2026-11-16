#!/usr/bin/env bash
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Compile sketches for native_sim, then run the ones that have expect logic
# and check their output. Exits with code 1 if any compile or check failed.
#
# Usage: run_sketches.sh [-r <report>] [-n <name root>] [-p <name prefix>]
#                        [-l <libraries dir>]... [-j <jobs>]
#                        [-u <unavailable list>]
#                        <sketch dir>...
#
#   -r  write a compile-sketches style report to this JSON file (default:
#       $SKETCH_REPORT, if set)
#   -n  directory the sketch names are relative to (default: .)
#   -p  prefix for the sketch names (default: none)
#   -l  libraries directory for arduino-cli (repeatable)
#   -j  number of sketches tested in parallel (default: the number of CPUs)
#   -u  file listing sketch directories that are not available on native_sim
#       (one per line): they are not built, only added to the report
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
JOBS=$(nproc)
UNAVAILABLE=
NAME_ROOT=.
NAME_PREFIX=
# the warning flags of test_core
CLI_ARGS=(--build-property "compiler.c.extra_flags=-Wno-type-limits -Wno-missing-field-initializers -Werror=return-type"
	  --build-property "compiler.cpp.extra_flags=-Wno-type-limits -Wno-missing-field-initializers -Werror=return-type")
while getopts "r:n:p:l:j:u:" opt ; do
	case $opt in
		r) REPORT=$OPTARG ;;
		n) NAME_ROOT=$OPTARG ;;
		p) NAME_PREFIX=$OPTARG ;;
		l) CLI_ARGS+=(--libraries "$(realpath "$OPTARG")") ;;
		j) JOBS=$OPTARG ;;
		u) UNAVAILABLE=$OPTARG ;;
		*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -gt 0 ] || [ -s "$UNAVAILABLE" ] || usage

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

# test_one <sketch dir> <sketch name> <work dir>: test a sketch, with its
# own counters and output directory, and write there its log (test.log),
# its check counts (counts) and its report entry (sketch.json)
test_one() {
	local dir=$1 name=$2 expect
	OUT=$3
	PASSED=0
	FAILED=0
	mkdir -p "$OUT"
	# (zephyr.exe writes its flash image in the current directory)
	cd "$OUT" || return
	expect=$(expect_script "$dir" "$name")

	sketch_test "$dir" "$expect" > "$OUT/test.log" 2>&1
	echo "$PASSED $FAILED" > "$OUT/counts"

	[ -n "$REPORT" ] || return 0
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
		> "$OUT/sketch.json"
}

# collect <index>: print the log of a completed sketch test, and add its
# results to the totals and the report
collect() {
	local i=$1 work=$BASE_OUT/$(($1 + 1)) passed failed
	# (a test that did not complete, e.g. if its expect script exited, is
	# a failure, also in the report)
	if ! read -r passed failed 2> /dev/null < "$work/counts" || { [ -n "$REPORT" ] && [ ! -f "$work/sketch.json" ] ; } ; then
		passed=${passed:-0}
		failed=$((${failed:-0} + 1))
		echo "FAIL: ${names[$i]}: the test did not complete" >> "$work/test.log"
		jq -n -c --arg name "${names[$i]}" '{
			name: $name,
			compilation_success: false,
			issues: [ "The test did not complete, see the CI log." ]
		}' > "$work/sketch.json"
	fi
	[ -z "${GITHUB_ACTIONS:-}" ] && echo "=== ${names[$i]}" || echo "::group::${names[$i]}"
	cat "$work/test.log"
	[ -z "${GITHUB_ACTIONS:-}" ] || echo "::endgroup::"
	PASSED=$((PASSED + passed))
	FAILED=$((FAILED + failed))
	TOTAL=$((TOTAL + 1))
	[ "$failed" -eq 0 ] || SKETCH_FAILED=$((SKETCH_FAILED + 1))
	[ ! -f "$work/sketch.json" ] || cat "$work/sketch.json" >> "$BASE_OUT/sketches.jsonl"
}

# flush: collect the sketch tests that completed, in the order of the
# sketches, so that the logs are shown as soon as possible
flush() {
	while [ "$NEXT" -lt "${#pids[@]}" ] && ! kill -0 "${pids[$NEXT]}" 2> /dev/null ; do
		collect "$NEXT"
		NEXT=$((NEXT + 1))
	done
}

# test the sketches in parallel, up to $JOBS at a time, each building with
# all the compiler jobs (most sketches build few files, a large one left last
# gets all the CPUs); the first one alone, as it also builds the core that
# arduino-cli caches for the others
NAME_ROOT=$(realpath "$NAME_ROOT")
BASE_OUT=$OUT
TOTAL=0
SKETCH_FAILED=0
NEXT=0
names=()
pids=()
: > "$BASE_OUT/sketches.jsonl"
for dir in "$@" ; do
	dir=$(realpath "$dir")
	names+=("$NAME_PREFIX$(realpath --relative-to="$NAME_ROOT" "$dir")")
	(test_one "$dir" "${names[-1]}" "$BASE_OUT/${#names[@]}") &
	pids+=($!)
	[ ${#names[@]} -gt 1 ] || wait
	flush
	while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ] ; do
		wait -n
		flush
	done
done
while [ "$(jobs -rp | wc -l)" -gt 0 ] ; do
	wait -n
	flush
done
wait
flush
OUT=$BASE_OUT

# the sketches that are not available, reported as such
if [ -n "$UNAVAILABLE" ] ; then
	while read -r dir ; do
		[ -n "$dir" ] || continue
		name=$NAME_PREFIX$(realpath -m --relative-to="$NAME_ROOT" "$dir")
		echo "=== $name: not available on native_sim"
		jq -n -c --arg name "$name" '{ name: $name, available: false }' >> "$OUT/sketches.jsonl"
	done < "$UNAVAILABLE"
fi

echo "$SKETCH_FAILED of $TOTAL sketches failed"
echo "$PASSED passed, $FAILED failed"

if [ -n "$REPORT" ] ; then
	jq -s --arg board "$FQBN" '{ boards: [ { board: $board, sketches: . } ] }' \
		"$OUT/sketches.jsonl" > "$REPORT"
fi

[ "$FAILED" -eq 0 ]
