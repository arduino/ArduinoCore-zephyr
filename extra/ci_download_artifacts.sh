#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script downloads the artifacts of a workflow run whose names match
# some patterns, as actions/download-artifact does with 'pattern' and
# 'merge-multiple'. That action, when one of many downloads is retried, may
# keep waiting for minutes after it finished; gh run download does not retry
# and cannot extract the artifacts uploaded with 'archive: false'.
#
# Each artifact is fetched from the REST API, retried on errors, and checked
# against its digest. Zipped artifacts are extracted, the others (uploaded
# with 'archive: false') are saved as a file named after the artifact. All
# land in the same output directory: later files overwrite earlier ones.
#
# Environment:
#  - GH_TOKEN (or GITHUB_TOKEN): a token with 'actions: read' on the repository
#  - GITHUB_REPOSITORY: owner/repo of the run
#  - GITHUB_RUN_ID: the run, unless given with -r
#  - GITHUB_API_URL: API endpoint (default: https://api.github.com)
#
# Usage: ci_download_artifacts.sh [-r <run id>] <output dir> <pattern>...
#  <pattern> is a shell glob matched against the artifact names. Matching no
#  artifact is not an error; failing to download one is, after trying all.
#  The artifacts are downloaded in parallel.

set -e -o pipefail

RUN_ID=${GITHUB_RUN_ID:-}
if [ "$1" == "-r" ]; then
	RUN_ID=$2
	shift 2
fi

if [ "$#" -lt 2 ] || [ -z "$RUN_ID" ] || [ -z "${GITHUB_REPOSITORY:-}" ]; then
	echo "Usage: $0 [-r <run id>] <output dir> <pattern>... (with GITHUB_REPOSITORY and GITHUB_RUN_ID set)" >&2
	exit 1
fi

OUT_DIR=$1
shift
TOKEN=${GH_TOKEN:-${GITHUB_TOKEN:-}}
API=${GITHUB_API_URL:-https://api.github.com}/repos/$GITHUB_REPOSITORY/actions
ATTEMPTS=5
JOBS=8

api() {
	curl -sSfL --retry 3 -H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json" "$@"
}

# "<id> <digest> <name>" of the non-expired artifacts of the run, the latest
# one for each name
list_artifacts() {
	local page=1 count
	while : ; do
		json=$(api "$API/runs/$RUN_ID/artifacts?per_page=100&page=$page")
		count=$(jq '.artifacts | length' <<< "$json")
		jq -r '.artifacts[] | select(.expired | not) | "\(.id) \(.digest // "-") \(.name)"' <<< "$json"
		[ "$count" -eq 100 ] || break
		page=$((page + 1))
	done | sort -k1,1nr | sort -s -u -k3,3
}

# download <id> <digest> <name>: download and unpack an artifact to OUT_DIR
download() {
	local id=$1 digest=$2 name=$3 file type
	file=$WORK/$id
	type=$(api -o "$file" -w '%{content_type}' "$API/artifacts/$id/zip") || return 1
	if [ "$digest" != "-" ] && [ "sha256:$(sha256sum "$file" | cut -d' ' -f1)" != "$digest" ]; then
		echo "$name: digest mismatch"
		return 1
	fi
	case "$type" in
		application/zip*) unzip -q -o "$file" -d "$OUT_DIR" ;;
		*) cp "$file" "$OUT_DIR/$name" ;;
	esac
}

mkdir -p "$OUT_DIR"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# fetch <id> <digest> <name>: download an artifact, retrying on errors
fetch() {
	local attempt
	for attempt in $(seq $ATTEMPTS); do
		if download "$@"; then
			echo "Downloaded $3"
			return
		elif [ "$attempt" -lt $ATTEMPTS ]; then
			echo "$3: download failed, retrying"
			sleep $((attempt * 5))
		fi
	done
	echo "::error::Cannot download artifact $3"
	touch "$WORK/$1.failed"
}

# the matching artifacts, JOBS at a time
TOTAL=0
while read -r id digest name ; do
	match=
	for pattern in "$@"; do
		# shellcheck disable=SC2053 # glob match intended
		[[ $name == $pattern ]] && match=1 && break
	done
	[ -n "$match" ] || continue
	TOTAL=$((TOTAL + 1))
	fetch "$id" "$digest" "$name" &
	while [ "$(jobs -rp | wc -l)" -ge $JOBS ] ; do
		wait -n || true
	done
done < <(list_artifacts)
wait
FAILED=$(find "$WORK" -name '*.failed' | wc -l)

echo "Downloaded $((TOTAL - FAILED)) of $TOTAL artifacts matching: $*"
[ "$FAILED" -eq 0 ]
