#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script generates a SemVer-compatible version number based on Git tags.
#
# If the current commit is tagged, it returns that version. If not, it
# generates a version string based on the next patch number and the current
# commit hash.
#
# If the tag is a simple "<maj>.<min>.<patch>", git describe will output:
#
#     <maj>.<min>.<patch>-<number-of-commits-since-tag>-g<commit-hash>
#
# A bash regex extracts the components and produces:
#
#     <maj>.<min>.<next-patch>-0.<label>
#
# The leading "0" is a numeric SemVer identifier that sorts below any
# alphanumeric pre-release label (alpha, beta, rc), ensuring non-pre-release
# development builds always sort lower than explicitly tagged pre-releases of
# the same version.
#
# If the tag refers to a pre-release, like "<maj>.<min>.<patch>-<extra-stuff>",
# the output will be:
#
#     <maj>.<min>.<patch>-<extra-stuff>.<label>
#
# This preserves the expected SemVer ordering when any of the above change.
# The label encodes the build context and is chosen so that SemVer ordering
# reflects trustworthiness: branch push < PR < tag push < local build:
#
#     Branch push: <label> = "head.<branch-name>.<count-from-tag>"
#     PR build:    <label> = "pr.<pr-number>.<date-time>"
#     Tag push:    <label> = "tag.<tag-name>"
#     Local build: <label> = "wip.<date-time>"
#
# These are among the lowest possible SemVer versions greater than the last
# tagged version. If there are no tags at all (for example when run in a fork
# etc), it defaults to "9.9.9-<date-time>".
#
# Finally, non-exact-tag versions have a "+g<commit-hash[-dirty]>" appended. The
# commit hash used is taken from HEAD_REF if set - this is to ensure that in CI
# environments the correct commit is used, even in pull requests where HEAD
# might be a temporary detached commit.

# The computed version is also written to a file in the Zephyr-compatible
# VERSION format if a filename is provided as the first argument.

# Determine pre-release label based on build context
DATE="$(date -u '+%y%m%dt%H%Mz')"
if [ -z "$GITHUB_ACTIONS" ]; then
	# local build, highest precedence
	KIND="wip"
	NAME="$DATE"
elif [ "$GITHUB_EVENT_NAME" == "pull_request" ]; then
	# CI build for a PR, medium precedence
	KIND="pr"
	NAME="$(jq -r .number < "$GITHUB_EVENT_PATH").$DATE"
elif [[ "$GITHUB_REF" =~ ^refs/(heads|tags)/(.*) ]]; then
	# CI build for a branch or tag push, lowest precedence
	KIND="${BASH_REMATCH[1]%s}" # "heads" -> "head", "tags" -> "tag"
	NAME="${BASH_REMATCH[2]//\//-}" # replace slashes with dashes
else
	# unknown CI context
	echo "Error: unexpected GITHUB_REF '$GITHUB_REF'" >&2
	exit 1
fi

if [ -n "$PINNED_CORE_VERSION" ] ; then
	# If PINNED_CORE_VERSION is set, use it as the version (extract fields)
	# must match <maj>.<min>.<patch>(-<prerel>)(+<buildinfo>)
	pattern='^([0-9]+)\.([0-9]+)\.([0-9]+)(-[^+]*)?(\+.*)?'
	if [[ $PINNED_CORE_VERSION =~ $pattern ]]; then
		v_maj="${BASH_REMATCH[1]}"
		v_min="${BASH_REMATCH[2]}"
		v_patch="${BASH_REMATCH[3]}"
		v_extra="${BASH_REMATCH[4]}" # optional, lead -
		v_tweak="${BASH_REMATCH[5]}" # optional, lead +
	else
		echo "Error: unexpected pinned ver '$PINNED_CORE_VERSION'" >&2
		exit 1
	fi
else
	exact_version=$(git describe --tags --exact-match --exclude '*/*' --exclude 'windows_toolchain_*' 2>/dev/null)
	if [ -n "$exact_version" ] ; then
		# this is a tagged build, extract the version components from the tag
		# must match <maj>.<min>.<patch>(-<prerel>)
		pattern='^([0-9]+)\.([0-9]+)\.([0-9]+)(-.*)?'
		if [[ $exact_version =~ $pattern ]]; then
			v_maj="${BASH_REMATCH[1]}"
			v_min="${BASH_REMATCH[2]}"
			v_patch="${BASH_REMATCH[3]}"
			v_extra="${BASH_REMATCH[4]}" # optional, lead -
		else
			echo "Error: unexpected tag '$exact_version'" >&2
			exit 1
		fi

		# no additional information
		v_tweak=""
	else
		version_from_git=$(git describe --tags --long --exclude '*/*' --exclude 'windows_toolchain_*' 2>/dev/null)
		# must match <maj>.<min>.<patch>(-<prerel>)-<number-of-commits-since-tag>-g<commit-hash>
		pattern='^([0-9]+)\.([0-9]+)\.([0-9]+)(-.*)?-([0-9]+)-g.*'
		if [[ $version_from_git =~ $pattern ]]; then
			v_maj="${BASH_REMATCH[1]}"
			v_min="${BASH_REMATCH[2]}"
			v_patch="${BASH_REMATCH[3]}"
			v_extra="${BASH_REMATCH[4]}" # optional, lead -
			count="${BASH_REMATCH[5]}"
		elif [ -z "$version_from_git" ]; then
			# no matching tag reachable at all (eg. a fresh branch history)
			v_maj=9
			v_min=9
			v_patch=9
			v_extra="-0"
			count=0
		else
			echo "Error: unexpected git describe output '$version_from_git'" >&2
			exit 1
		fi

		if [ -z "${v_extra}" ]; then
			v_patch=$((v_patch+1))
			v_extra="-0"
		fi

		if [ "$KIND" == "head" ] ; then
			# number of commits since tag is only relevant for a branch push
			v_extra="${v_extra}.${KIND}.${NAME}.${count}"
		else
			v_extra="${v_extra}.${KIND}.${NAME}"
		fi

		# If HEAD_REF is not set, we're not in CI but in a local clone. Use the
		# implicit HEAD and include --dirty to test for uncommitted changes.
		v_tweak="+g$(git describe --always ${HEAD_REF:---dirty})"
	fi
fi

VERSION="${v_maj}.${v_min}.${v_patch}${v_extra}${v_tweak}"
echo $VERSION

if [ -n "$1" ]; then
	# Write the version components to a file in Zephyr-compatible format,
	# removing leading separators from EXTRAVERSION and VERSION_TWEAK.
	cat > $1 << EOF
# This file is auto-generated by get_core_version.sh. Do not edit.
# Generated from version string: ${VERSION}

VERSION_MAJOR = ${v_maj}
VERSION_MINOR = ${v_min}
PATCHLEVEL = ${v_patch}
EXTRAVERSION = ${v_extra#-}
VERSION_TWEAK = ${v_tweak#+}
EOF
fi
