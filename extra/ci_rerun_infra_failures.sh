#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script checks a completed CI workflow run and re-runs its failed jobs
# if any of them failed because of an infrastructure issue, such as a network
# timeout while transferring the build environment.
#
# A failed job is considered an infrastructure failure if all its failed steps
# are in the INFRA_STEPS list below, or if it failed without any failed step
# (lost runner, setup error). The failed jobs are re-run if at least one of
# them is an infrastructure failure: the other failures are usually caused by
# the same issue, and real errors will just fail again.
#
# The failed jobs are not re-run if:
#  - the run is not completed, or already reached MAX_ATTEMPTS attempts;
#  - a board build must be re-run, but the build environment it needs has
#    expired (only re-running all jobs can work then);
#  - the run is outdated: the pull request or branch it tested has changed.
#
# Expects the GH_TOKEN, GITHUB_REPOSITORY and RUN_ID environment variables.
# RUN_ATTEMPT selects the attempt to check (default: the latest one).
# If DRY_RUN is set, the decision is printed but no job is re-run.
#
# Usage: ci_rerun_infra_failures.sh

set -e

MAX_ATTEMPTS=${MAX_ATTEMPTS:-3}
INFRA_STEPS='^(Run actions/(checkout|download-artifact)@.*|Install OS dependencies|Restore build environment|Initialize Zephyr environment)$'

REPO=$GITHUB_REPOSITORY
SUMMARY=${GITHUB_STEP_SUMMARY:-/dev/null}

log() {
	echo "$1" | tee -a "$SUMMARY"
}

RUN=$(gh api "repos/$REPO/actions/runs/$RUN_ID")
ATTEMPT=${RUN_ATTEMPT:-$(jq -r '.run_attempt' <<< "$RUN")}
STATUS=$(jq -r '.status' <<< "$RUN")
EVENT=$(jq -r '.event' <<< "$RUN")
HEAD_SHA=$(jq -r '.head_sha' <<< "$RUN")
HEAD_BRANCH=$(jq -r '.head_branch' <<< "$RUN")
RUN_URL=$(jq -r '.html_url' <<< "$RUN")

log "### Run [$RUN_ID]($RUN_URL), attempt $ATTEMPT ($EVENT on \`$HEAD_BRANCH\`)"
log ""

# Classify the failed jobs of the last attempt
JOBS=$(gh api --paginate "repos/$REPO/actions/runs/$RUN_ID/attempts/$ATTEMPT/jobs?per_page=100" --jq '.jobs[]' | jq -cs --arg infra "$INFRA_STEPS" '
	map(select(.conclusion == "failure" or .conclusion == "timed_out")
	    | ([ .steps[]? | select(.conclusion == "failure" or .conclusion == "timed_out") | .name ]) as $steps
	    | { name, steps: $steps, infra: all($steps[]; test($infra)) })
')

jq -r '.[] | "- \(if .infra then ":construction:" else ":x:" end) \(.name): \(if (.steps | length) > 0 then .steps | join(", ") else "no failed step" end)"' <<< "$JOBS" | tee -a "$SUMMARY"
log ""

skip() {
	log "**Not re-running:** $1"
	exit 0
}

[ "$(jq 'map(select(.infra)) | length' <<< "$JOBS")" -gt 0 ] || skip "no infrastructure failure found."
[ "$STATUS" == "completed" ] || skip "the run is already $STATUS."
[ "$ATTEMPT" -lt "$MAX_ATTEMPTS" ] || skip "already at attempt $ATTEMPT of $MAX_ATTEMPTS."

if [ "$(jq 'map(select(.name | startswith("Build for"))) | length' <<< "$JOBS")" -gt 0 ]; then
	BUILD_ENV=$(gh api "repos/$REPO/actions/runs/$RUN_ID/artifacts?name=build-env.tar.zst" --jq '[.artifacts[] | select(.expired | not)] | length')
	[ "$BUILD_ENV" -gt 0 ] || skip "the build environment has expired, all jobs must be re-run."
fi

case "$EVENT" in
	pull_request)
		# the run is current if an open pull request still has the tested commit as its head
		OPEN_PRS=$(gh api --paginate "repos/$REPO/pulls?state=open&per_page=100" --jq ".[] | select(.head.sha == \"$HEAD_SHA\") | .number")
		[ -n "$OPEN_PRS" ] || skip "the pull request has changed or was closed."
		;;
	push)
		# tag pushes have no branch to compare with
		TIP=$(gh api "repos/$REPO/branches/$HEAD_BRANCH" --jq '.commit.sha' 2>/dev/null || true)
		[ -z "$TIP" ] || [ "$TIP" == "$HEAD_SHA" ] || skip "branch \`$HEAD_BRANCH\` has new commits."
		;;
esac

if [ -n "$DRY_RUN" ]; then
	log "**Would re-run** the failed jobs (dry run)."
else
	gh run rerun "$RUN_ID" --failed -R "$REPO"
	log "**Re-running** the failed jobs as attempt $((ATTEMPT + 1))."
fi
