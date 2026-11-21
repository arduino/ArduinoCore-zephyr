# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Shared helpers for the scripts in extra/. Source this file, do not run it.

# Usage: log_msg <group|endgroup|notice|warning|error> [message]
#
# In GitHub Actions, emits the message as a workflow command so it is rendered
# as a collapsible group or an annotation. Elsewhere, prints plain text.
log_msg() {
	if [ -n "${GITHUB_WORKSPACE:-}" ] ; then
		echo "::$1::${2:-}"
		return
	fi

	case "$1" in
		group)    echo " - ${2:-}" ;;
		endgroup) ;;
		*)        echo "$1: ${2:-}" ;;
	esac
}

# Usage: declutter_file <file>
#
# Prints the entries of an artifact file (extra/artifacts/*.inc, .exc, .only),
# without comments, trailing whitespace, '/' dir terminators and empty lines.
declutter_file() {
	[ -f "$1" ] || return 0
	sed -e 's/\s*#.*//' -e 's/\s*$//' -e 's/\/$//' "$1" | grep -v '^\s*$' || true
}

# Usage: artifact_exclusions <artifact>
#
# Prints the paths left out of the core package of an artifact: the common
# and artifact .exc entries, and the .only entries of the other artifacts.
# native_sim poses as the boards of all artifacts: it ships their own files.
artifact_exclusions() {
	declutter_file extra/artifacts/_common.exc
	declutter_file "extra/artifacts/$1.exc"
	[ "$1" == "native_sim" ] && return
	for f in extra/artifacts/*.only ; do
		[ "$f" == "extra/artifacts/$1.only" ] || declutter_file "$f"
	done
}
