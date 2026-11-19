#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script downloads all external test repositories referenced by all
# test_setup.sh files (artifact-level and variant-level). Each repo is
# downloaded as a complete archive into the test-data/ directory.
#
# The downloaded archives are later consumed by ci_apply_tests.sh, which
# extracts only the paths relevant for a specific board variant.
#
# This script is intended to run once per workflow, before the per-board
# test jobs. Run from the repository root (needs extra/ and variants/).
# GH_TOKEN (or GITHUB_TOKEN), if set, authenticates the GitHub API requests.

set -e

if [ ! -d extra/artifacts/ ]; then
	echo "Must be run from the ArduinoCore-zephyr root directory."
	exit 1
fi

DOWNLOAD_DIR="test-data"
mkdir -p "$DOWNLOAD_DIR"

get_repo_key() {
	# $1: type of fetch
	# $2: repo (owner/name)
	echo "${1}__${2//\//__}"
}

get_latest_release() {
	# $1: folder (ignored at fetch time)
	# $2: repo (owner/name)
	# $3+: paths (ignored at fetch time)
	local repo="$2"
	local key="$(get_repo_key release "$repo")"

	local file="${DOWNLOAD_DIR}/${key}.tar.gz"
	[ -f "${file}" ] && return # already downloaded

	# (authenticated if a token is available: anonymous API requests are
	# rate limited per IP, shared by many runners)
	local token=${GH_TOKEN:-${GITHUB_TOKEN:-}}
	local release=$(curl -sS --retry 3 ${token:+-H "Authorization: Bearer $token"} "https://api.github.com/repos/${repo}/releases/latest")
	local url=$(jq -r '.tarball_url // empty' <<< "$release" 2>/dev/null)
	if [ -z "$url" ] ; then
		echo "::error::No releases found for ${repo}: $(jq -r '.message // empty' <<< "$release" 2>/dev/null)"
		exit 1
	fi

	# (the tarball URL is an API one too; the token is not sent on the
	# redirect to the download host)
	echo "::group::Downloading latest release of ${repo}"
	curl -sSfL --retry 3 ${token:+-H "Authorization: Bearer $token"} -o "${file}" "$url"
	echo "::endgroup::"
}

get_branch_tip() {
	# $1: folder (ignored at fetch time)
	# $2: repo (owner/name)
	# $3: branch
	# $4+: paths (ignored at fetch time)
	local repo="$2"
	local branch="$3"
	local key="$(get_repo_key "branch_$branch" "$repo")"

	local file="${DOWNLOAD_DIR}/${key}.tar.gz"
	[ -f "${file}" ] && return # already downloaded

	local url="https://github.com/${repo}/archive/refs/heads/${branch}.tar.gz"

	echo "::group::Downloading branch ${branch} of ${repo}"
	wget -nv "$url" -O "${file}"
	echo "::endgroup::"
}

skip_for_this_board() {
	: # no-op at fetch time
}

# Source all test_setup.sh files to pick up any package references
# (order doesn't matter since we only gather files at fetch time)

for artifact_setup in extra/artifacts/*.test_setup.sh; do
	. "$artifact_setup"
done
for variant_setup in variants/*/test_setup.sh; do
	. "$variant_setup"
done
