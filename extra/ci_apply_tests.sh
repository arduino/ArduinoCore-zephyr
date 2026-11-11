#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script builds the per-board test list for GitHub Actions CI.
# It consumes the pre-downloaded archives in test-data/ (produced by
# ci_fetch_tests.sh) and writes the lists for the current board variant to
# <list dir> as plain text files, one entry per line: tests.txt (sketch
# directories) and libraries.txt (library dependencies).
#
# The core under test must be extracted in the 'ArduinoCore-zephyr'
# subdirectory, and test-data/ must be present in the working directory.

set -e

if [ "$#" -ne 3 ]; then
	echo "Usage: $0 <artifact> <variant> <list dir>"
	exit 1
fi

if [ ! -d ArduinoCore-zephyr/ ] || [ ! -d test-data/ ]; then
	echo "Missing prerequisites (ArduinoCore-zephyr/, test-data/)."
	exit 1
fi

ARTIFACT=$1
VARIANT="$2"
LIST_DIR="$3"
mkdir -p "$LIST_DIR"

ALL_TESTS=$(mktemp)

get_repo_key() {
	# $1: type of fetch
	# $2: repo (owner/name)
	echo "${1}__${2//\//__}"
}

search_for_sketches_in() {
	local folder="$1"
	find "$folder" -name "*.ino" 2>/dev/null | sed -e 's/^\.\///'
}

extract_and_search() {
	# $1: archive path
	# $2: inner folder glob pattern (e.g. "*-project-*" or "project-branch")
	# $3: output folder (destination after extraction)
	# $4+: sub-paths to search within the output folder (empty = whole folder)
	local archive="$1"
	local inner_pattern="$2"
	local output_folder="$3"
	shift 3

	if [ ! -d "${output_folder}" ]; then
		local temp_dir
		temp_dir=$(mktemp -d)
		tar -xzf "$archive" -C "$temp_dir"
		local inner_folder
		inner_folder=$(ls -d "${temp_dir}/"${inner_pattern} 2>/dev/null | head -1)

		mkdir -p "$(dirname "$output_folder")"
		mv "$inner_folder" "$output_folder"
		rm -rf "$temp_dir"
	fi

	if [ $# -eq 0 ]; then
		search_for_sketches_in "$output_folder" >> "$ALL_TESTS"
	else
		for item in "$@"; do
			search_for_sketches_in "$output_folder/${item}" >> "$ALL_TESTS"
		done
	fi
}

get_latest_release() {
	local folder="$1"
	local repo="$2"
	local project="${repo##*/}"
	local key="$(get_repo_key release "$repo")"
	shift 2

	echo "Extracting latest release of ${repo}"
	extract_and_search "test-data/${key}.tar.gz" "*-${project}-*" \
		"ArduinoCore-zephyr/${folder}/${project}" "$@"
}

get_branch_tip() {
	local folder="$1"
	local repo="$2"
	local branch="$3"
	local project="${repo##*/}"
	local key="$(get_repo_key "branch_$branch" "$repo")"
	shift 3

	echo "Extracting branch ${branch} of ${repo}"
	extract_and_search "test-data/${key}.tar.gz" "${project}-${branch}" \
		"ArduinoCore-zephyr/${folder}/${project}" "$@"
}

skip_for_this_board() {
	local pattern="$1"
	sed -i -e "\\|^\\(ArduinoCore-zephyr/\\)\\?${pattern}|d" "$ALL_TESTS"
}

# Collect built-in tests from the core
search_for_sketches_in ArduinoCore-zephyr/libraries/ >> "$ALL_TESTS"
search_for_sketches_in ArduinoCore-zephyr/examples/ >> "$ALL_TESTS"

# Source common and artifact-specific setups to extract external repos and
# add their examples, then apply the variant-specific setup to filter results
[ -f extra/artifacts/_common.test_setup.sh ] && \
	. extra/artifacts/_common.test_setup.sh
[ -f "extra/artifacts/${ARTIFACT}.test_setup.sh" ] && \
	. "extra/artifacts/${ARTIFACT}.test_setup.sh"
[ -f "variants/${VARIANT}/test_setup.sh" ] && \
	. "variants/${VARIANT}/test_setup.sh"

find ArduinoCore-zephyr/libraries/ -name library.properties | while read -r propfile; do
	# Version constraints are ignored as they are not supported by compile-sketches
	grep '^depends=' "$propfile" | cut -d= -f2- | tr ',' '\n' | \
		sed -e 's/\s*(.*)\s*$//' | while read -r dep; do
		[ -z "$dep" ] || echo "$dep"
	done
done > "$LIST_DIR/libraries.txt"

# output the collected test list for this variant to both the console and
# the list file
echo -e "\nCollected tests for ${VARIANT}:"
while read -r infile; do
	dirname "$infile"
done < "$ALL_TESTS" | sort | tee "$LIST_DIR/tests.txt"
