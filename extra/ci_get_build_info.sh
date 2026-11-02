#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script computes the metadata describing a CI core build: versions,
# board set and reference package index. It is shared by all CI workflows
# that need it, so that they all agree on the same values.
#
# Expects a full checkout with tags, and the standard GitHub Actions
# environment variables (GITHUB_REF_TYPE, GITHUB_REF_NAME, GITHUB_BASE_REF,
# GITHUB_REPOSITORY). Results are appended to $GITHUB_ENV, or printed to
# stdout if that is not set:
#
#  - CORE_HASH: hash identifying this core build, for exact version match
#  - CORE_VER: user-friendly SemVer-compatible version for the build
#  - CORE_TAG: tag (if found) or hash, for user-friendly strings and file names
#  - CORE_BRANCH: target branch of this build, used for ci-builds tracking
#  - ALL_BOARD_DATA: JSON describing all boards, from get_board_details.sh
#  - ALL_BOARD_FQBNS: ALL_BOARD_DATA with link_mode alternatives added, each
#    requiring the package of its artifact
#  - ARTIFACTS: list of artifacts, including the "zephyr" umbrella artifact
#  - PACKAGE_DATA: ARTIFACTS entries, each requiring the builds of its boards
#  - SUB_ARCHES: list of architectures the artifacts are built for
#  - CI_BUILDS_URL: latest ci-builds index for this branch (official repo only)
#
# Each entry of ALL_BOARD_DATA, PACKAGE_DATA and ALL_BOARD_FQBNS has an 'id'
# naming the job built from it: "build-<board>", "package-<artifact>" and
# "test-<board>-<link_mode>" respectively. PACKAGE_DATA and ALL_BOARD_FQBNS
# entries list the ids of the jobs they depend on in their 'requires' field.
# See ci_filter_matrix.sh.
#
# Usage: ci_get_build_info.sh [<ref>]
#  <ref> is the commit used for CORE_HASH (defaults to HEAD).

set -e

if [ ! -d extra/artifacts/ ]; then
	echo "Must be run from the ArduinoCore-zephyr root directory."
	exit 1
fi

REF=${1:-HEAD}
OUTPUT=${GITHUB_ENV:-/dev/stdout}

CORE_HASH=$(git describe --always $REF)
ALL_BOARD_DATA=$(extra/get_board_details.sh | jq -c 'sort_by(.variant) | map(. + {id: "build-\(.board)"})')
CORE_VER=$(extra/get_core_version.sh)
CORE_TAG=$(git describe --tags --exact-match --exclude '*/*' 2>/dev/null || echo $CORE_HASH)
if [[ "$GITHUB_REF_TYPE" == "tag" ]]; then
	CORE_BRANCH=$(git branch -r --contains HEAD --sort=-committerdate --format='%(refname:short)' | sed 's|origin/||' | grep -v HEAD | head -1)
	[[ -n "$CORE_BRANCH" ]] || { echo "::error::No branch found containing tag $GITHUB_REF_NAME"; exit 1; }
else
	CORE_BRANCH=${GITHUB_BASE_REF:-$GITHUB_REF_NAME}
fi
ALL_BOARD_FQBNS=$(jq -c 'map(. + {requires: ["package-\(.artifact)"]}) | map((. + {link_mode: "static"}), (. + {link_mode: "dynamic"})) | map(. + {id: "test-\(.board)-\(.link_mode)"})' <<< ${ALL_BOARD_DATA})
ARTIFACTS=$(jq -c '["zephyr"] + (map(.artifact) | unique)' <<< ${ALL_BOARD_DATA})
PACKAGE_DATA=$(jq -c --argjson artifacts "$ARTIFACTS" '. as $boards | $artifacts | map(. as $artifact | {
		id: "package-\($artifact)",
		artifact: $artifact,
		requires: [ $boards[] | select($artifact == "zephyr" or .artifact == $artifact) | .id ]
	})' <<< ${ALL_BOARD_DATA})
SUB_ARCHES=$(jq -c 'map(.subarch) | unique' <<< ${ALL_BOARD_DATA})

CI_BUILDS_URL=
if [[ "$GITHUB_REPOSITORY" == "arduino/ArduinoCore-zephyr" ]]; then
	# for pull requests and pushes to the official Arduino repo, use the latest ci-built core as a reference
	CI_BUILDS_URL="https://arduino.github.io/core-ci-builds/package_arduino_zephyr_${CORE_BRANCH}_ci_index.json"
	if ! wget -q --method=HEAD "$CI_BUILDS_URL" >& /dev/null ; then
		CLOSEST_BRANCH=$(git branch -r | grep -vE "HEAD|/$(git branch --show-current)$" | while read -r b; do \
		                   echo "$(git rev-list --count $b..HEAD) $b" ; \
		                 done | sort -n | head -n 1 | awk '{print $2}' | cut -d / -f 2)
		[ -z "$CLOSEST_BRANCH" ] && CLOSEST_BRANCH="main"
		echo "::warning::No ci-builds index found for branch $CORE_BRANCH, using $CLOSEST_BRANCH"
		CI_BUILDS_URL="https://arduino.github.io/core-ci-builds/package_arduino_zephyr_${CLOSEST_BRANCH}_ci_index.json"
		if ! wget -q --method=HEAD "$CI_BUILDS_URL" >& /dev/null ; then
			echo "::warning::No ci-builds index found for branch $CLOSEST_BRANCH, using main"
			CI_BUILDS_URL="https://arduino.github.io/core-ci-builds/package_arduino_zephyr_main_ci_index.json"
		fi
	fi
fi

for VAR in CORE_HASH CORE_VER CORE_TAG CORE_BRANCH ALL_BOARD_DATA ALL_BOARD_FQBNS ARTIFACTS PACKAGE_DATA SUB_ARCHES CI_BUILDS_URL; do
	echo "$VAR=${!VAR}" >> "$OUTPUT"
done
