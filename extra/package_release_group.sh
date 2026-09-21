#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

if [ $# -ne 3 ]; then
	echo "Usage: $0 <release_group> <input_archive> <output_archive>" >&2
	exit 1
fi

RELEASE_GROUP=$1
INPUT_ARCHIVE=$2
OUTPUT_ARCHIVE=$3

if [ ! -f "$INPUT_ARCHIVE" ]; then
	echo "Input archive '$INPUT_ARCHIVE' not found." >&2
	exit 2
fi

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

tar -xf "$INPUT_ARCHIVE" -C "$WORK_DIR"
CORE_DIR="$WORK_DIR/ArduinoCore-zephyr"

if [ ! -f "$CORE_DIR/boards.txt" ]; then
	echo "Archive does not contain ArduinoCore-zephyr/boards.txt." >&2
	exit 3
fi

GROUP_BOARDS=$(sed -n "s/^\([^.#[:space:]]*\)\.build\.release_group=${RELEASE_GROUP}\$/\1/p" "$CORE_DIR/boards.txt")
if [ -z "$GROUP_BOARDS" ]; then
	echo "No boards found for release group '$RELEASE_GROUP'." >&2
	exit 4
fi

GROUP_VARIANTS=""
while read -r board; do
	[ -z "$board" ] && continue
	variant=$(sed -n "s/^${board}\.build\.variant=\(.*\)\$/\1/p" "$CORE_DIR/boards.txt")
	GROUP_VARIANTS="${GROUP_VARIANTS}${variant}"$'\n'
done <<< "$GROUP_BOARDS"

is_group_board() {
	grep -qx "$1" <<< "$GROUP_BOARDS"
}

is_group_variant() {
	grep -qx "$1" <<< "$GROUP_VARIANTS"
}

while read -r board; do
	[ -z "$board" ] && continue
	is_group_board "$board" && continue
	sed -i "/^${board}[.]/d" "$CORE_DIR/boards.txt"
done < <(sed -n 's/^\([^.#[:space:]]*\)\.build\.variant=.*/\1/p' "$CORE_DIR/boards.txt")

for variant_dir in "$CORE_DIR"/variants/*/; do
	variant_name=$(basename "$variant_dir")
	is_group_variant "$variant_name" || rm -rf "$variant_dir"
done

for firmware_file in "$CORE_DIR"/firmwares/*; do
	[ -e "$firmware_file" ] || continue
	basename_file=$(basename "$firmware_file")
	keep=false
	while read -r variant; do
		[ -z "$variant" ] && continue
		[[ "$basename_file" == "zephyr-${variant}."* ]] && keep=true
	done <<< "$GROUP_VARIANTS"
	$keep || rm -f "$firmware_file"
done

mkdir -p "$(dirname "$OUTPUT_ARCHIVE")"
tar -cjf "$OUTPUT_ARCHIVE" -C "$WORK_DIR" ArduinoCore-zephyr
