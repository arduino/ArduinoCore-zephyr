# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Helpers for the native_sim tests, sourced by run_sketches.sh and by the
# expect script of each sketch.
#
# Environment:
#   FQBN               board to compile for (default: arduino-git:zephyr:native_sim)
#   CORE_DIR           core with the built loader (default: the repo root)
#   VARIANT            loader variant (default: native_sim_native_64)
#   ARDUINO_CLI_FLAGS  extra arguments for 'arduino-cli compile'

TESTS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
FQBN=${FQBN:-arduino-git:zephyr:native_sim}
CORE_DIR=${CORE_DIR:-$(cd "$TESTS_DIR/../../.." && pwd)}
VARIANT=${VARIANT:-native_sim_native_64}
EXE=${CORE_DIR}/firmwares/zephyr-${VARIANT}.exe

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

PASSED=0
FAILED=0

# cap_log [<file>]: print the file (default: stdin) with at most its first 50
# and last 150 lines, each of at most 500 characters: the logs end up in the
# job log and in the CI summary, which is limited to 1 MiB
cap_log() {
	awk -v h=50 -v t=150 -v w=500 '
		{ $0 = substr($0, 1, w) }
		NR <= h { print; next }
		{ buf[NR % t] = $0 }
		END {
			first = h + 1
			if (NR > h + t) {
				printf "[... %d lines omitted ...]\n", NR - h - t
				first = NR - t + 1
			}
			for (i = first; i <= NR; i++)
				print buf[i % t]
		}' "${1:--}"
}

# report <status> <description> [<details file>]: record a check result
report() {
	if [ "$1" == "pass" ]; then
		PASSED=$((PASSED + 1))
		echo "PASS: $2"
	else
		FAILED=$((FAILED + 1))
		echo "FAIL: $2"
		[ $# -lt 3 ] || cap_log "$3"
	fi
}

# sketch_id <name>: the sketch name as a file name ('/' replaced by '_')
sketch_id() {
	echo "${1//\//_}"
}

# cli_property <key>: value of a build property for the native_sim board
cli_property() {
	# shellcheck disable=SC2086 # ARDUINO_CLI_FLAGS is a list of arguments
	arduino-cli compile -b "$FQBN" ${ARDUINO_CLI_FLAGS:-} --show-properties "$TESTS_DIR/exit3" |
		sed -n -e "s|^$1=||p"
}

# compile <sketch dir> [<arduino-cli args>...]: build the sketch, set SKETCH
# to its loadable binary
compile() {
	local dir=$1 name build
	shift
	name=$(basename "$dir")
	build=$OUT/build-$(sketch_id "$dir")
	SKETCH=$build/$name.ino.bin-zsk.bin
	# shellcheck disable=SC2086 # ARDUINO_CLI_FLAGS is a list of arguments
	if arduino-cli compile --no-color -b "$FQBN" ${ARDUINO_CLI_FLAGS:-} "$@" --output-dir "$build" "$dir" \
			> "$OUT/compile.log" 2>&1 ; then
		report pass "$name: compile"
	else
		report fail "$name: compile" "$OUT/compile.log"
		return 1
	fi
}

# crash_info [<zephyr.exe args>...]: run the loader again in gdb, if
# available, and print where it crashed
crash_info() {
	local elf=${SKETCH%.bin-zsk.bin}.elf sym=()
	command -v gdb > /dev/null || return 0
	# the sketch symbols, at their static link addresses
	[ ! -f "$elf" ] || sym=(-ex "add-symbol-file $elf")
	echo "--- gdb"
	timeout -s KILL 60 gdb -q -batch "${sym[@]}" -ex run -ex bt -ex 'info registers' -ex 'x/8i $pc' \
		--args "$EXE" "$@" 2>&1 < /dev/null | grep -v '^\[gpio\]'
}

# run <expected exit code> <description> [<zephyr.exe args>...]: run the
# loader, keeping its output in $OUT/stdout and $OUT/stderr
run() {
	local expected=$1 desc=$2 rc=0
	shift 2
	# zephyr.exe ignores SIGTERM
	timeout -s KILL 60 "$EXE" "$@" < /dev/null > "$OUT/stdout" 2> "$OUT/stderr" || rc=$?
	if [ $rc -eq "$expected" ] ; then
		report pass "$desc"
	else
		echo "exit code $rc, expected $expected" >> "$OUT/stderr"
		[ $rc -le 128 ] || crash_info "$@" >> "$OUT/stderr"
		report fail "$desc" <(cat "$OUT/stdout" "$OUT/stderr")
	fi
}

# expect <description> <stdout|stderr> <regex>: check the last run output
expect() {
	if grep -qE "$3" "$OUT/$2" ; then
		report pass "$1"
	else
		report fail "$1" "$OUT/$2"
	fi
}

# run_loader_until <regex> [<zephyr.exe args>...]: run the loader until its
# output matches the extended regex, or for 60 seconds, keeping its output
# (without CRs) in $OUT/stdout and $OUT/stderr. For sketches that never
# exit: a busy loop() does not advance the simulated time, so -stop_at does
# not work, and zephyr.exe ignores SIGTERM.
run_loader_until() {
	local regex=$1 pid waited=0
	shift
	"$EXE" "$@" < /dev/null > "$OUT/raw" 2> "$OUT/stderr" &
	pid=$!
	while kill -0 $pid 2> /dev/null && ! grep -qE "$regex" "$OUT/raw" ; do
		if [ $waited -ge 600 ] ; then
			echo "timeout after 60 seconds" >> "$OUT/stderr"
			break
		fi
		sleep 0.1
		waited=$((waited + 1))
	done
	kill -9 $pid 2> /dev/null
	wait $pid 2> /dev/null
	tr -d '\r' < "$OUT/raw" > "$OUT/stdout"
}

# run_until <regex> <description> [<zephyr.exe args>...]: run the loader
# until its output matches the extended regex (see run_loader_until), and
# check that it did
run_until() {
	local regex=$1 desc=$2
	shift 2
	run_loader_until "$regex" "$@"
	if grep -qE "$regex" "$OUT/stdout" ; then
		report pass "$desc"
	else
		report fail "$desc" <(cat "$OUT/stdout" "$OUT/stderr")
	fi
}
