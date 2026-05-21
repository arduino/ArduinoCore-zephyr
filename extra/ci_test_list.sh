#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script generates a list of all libraries and their dependencies for use
# in GitHub Actions environment variables. It also generates a list of all example
# .ino files, excluding those specified in a variant's skip list.
#
# The core under test should be extracted in the 'ArduinoCore-zephyr' subdirectory.

set -e

if [ "$#" -lt 2 ] ; then
	echo "Usage: $0 <artifact> <variant> [<additional_library_paths>...]"
	exit 1
fi

if [ -z "$GITHUB_ENV" ] || [ ! -d ArduinoCore-zephyr/ ]; then
	echo "Not in a Github CI run, cannot proceed."
	exit 1
fi

ARTIFACT=$1
VARIANT_DIR="ArduinoCore-zephyr/variants/$2"
shift 2

search_for_sketches_in() {
	local folder="$1"
	find "$folder" -name *.ino 2>/dev/null | sed -e 's/^\.\///'
}

fetch_and_extract() {
	local temp_file=$(mktemp).tar.gz
	local temp_dir=$(mktemp -d)
	local link="$1"
	local inner_folder="${temp_dir}/$2"
	local output_folder="$3"
	shift 3

	wget -nv "$link" -O "$temp_file"
	tar -xzf "$temp_file" -C "$temp_dir"

	mkdir -p "$(dirname $output_folder)"
	mv $inner_folder "$output_folder"
	if [ $# -eq 0 ] ; then
		# Search entire project for tests
		search_for_sketches_in "$output_folder" >> $ALL_TESTS
	else
		# Search only specified paths for tests
		for item in "$@" ; do
			search_for_sketches_in "$output_folder/${item}" >> $ALL_TESTS
		done
	fi
	rm -rf $tmpdir
}

get_latest_release() {
	local folder="$1"
	local repo="$2"
	local project="${repo##*/}"
	local url=$(curl -s "https://api.github.com/repos/${repo}/releases/latest" | jq -r '.tarball_url')
	shift 2

	echo "::group::Getting latest release for ${repo}"

	fetch_and_extract "$url" "*-${project}-*" "ArduinoCore-zephyr/${folder}/${project}" "$@"

	echo "::endgroup::"
}

get_branch_tip() {
	local folder="$1"
	local repo="$2"
	local branch="$3"
	local project="${repo##*/}"
	local url="https://github.com/${repo}/archive/refs/heads/${branch}.tar.gz"
	shift 3

	echo "::group::Getting branch ${branch} of ${repo}"

	fetch_and_extract "$url" "${project}-${branch}" "ArduinoCore-zephyr/${folder}/${project}" "$@"

	echo "::endgroup::"
}

ALL_TESTS=$(mktemp)
search_for_sketches_in ArduinoCore-zephyr/libraries/ >> $ALL_TESTS
search_for_sketches_in ArduinoCore-zephyr/examples/ >> $ALL_TESTS

# Source common and artifact-specific scripts to get additional libraries in
[ -f extra/artifacts/_common.test_setup.sh ] && . extra/artifacts/_common.test_setup.sh
[ -f extra/artifacts/${ARTIFACT}.test_setup.sh ] && . extra/artifacts/${ARTIFACT}.test_setup.sh

echo "ALL_LIBRARIES<<EOF" >> $GITHUB_ENV
find ArduinoCore-zephyr/libraries/ -name library.properties | while read -r propfile; do
	# Version constraints are ignored as they are not supported by compile-sketches
	grep '^depends=' "$propfile" | cut -d= -f2- | tr ',' '\n' | sed -e 's/\s*(.*)\s*$//' | while read -r dep; do
		[ -z "$dep" ] || printf " - name: \"%s\"\n" "$dep" >> $GITHUB_ENV
	done
done
echo "EOF" >> $GITHUB_ENV

if [ -f $VARIANT_DIR/skip_these_examples.txt ] ; then
	cat $VARIANT_DIR/skip_these_examples.txt | sed -e 's/\s*#.*//' -e '/^\s*$/d' | while read -r pattern; do
		sed -i -e "\\|^\\(ArduinoCore-zephyr/\\)\\?${pattern}|d" $ALL_TESTS
	done
fi
echo "ALL_TESTS<<EOF" >> $GITHUB_ENV
cat $ALL_TESTS | while read -r infile; do
	printf " - \"%s\"\n" "$(dirname "$infile")" >> $GITHUB_ENV
done
echo "EOF" >> $GITHUB_ENV
