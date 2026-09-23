#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Native-builds each feature board's own ci_native_samples (as declared by
# <board>.build.feature_board=true and <board>.build.ci_native_samples in
# boards.txt), on top of the fixed set of samples ifx_build.yml already
# builds for other boards.
#
# Called as: bash extra/ci_build_native_samples.sh <module-path>
# Invoking it via "bash <script>" (rather than a plain run: block) ensures it
# always runs under bash, regardless of the job's default shell.

set -euo pipefail

MODULE_PATH="$(cd "${1:?Usage: $0 <module-path>}" && pwd)"

if ! command -v jq >/dev/null; then
	apt-get update -qq
	apt-get install -y --no-install-recommends jq
fi

(cd "$MODULE_PATH" && ./extra/get_board_details.sh) | \
	jq -c '.[] | select(.feature_board == true and .ci_native_samples != "")' | \
while IFS= read -r board_json; do
	target=$(jq -r '.target' <<< "$board_json")
	samples=$(jq -r '.ci_native_samples | split(",")[]' <<< "$board_json")
	while IFS= read -r sample; do
		echo "Building $target native sample: $sample"
		west build -p -b "$target" "$MODULE_PATH/samples/$sample"
	done <<< "$samples"
done
