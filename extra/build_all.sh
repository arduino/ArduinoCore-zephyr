#!/bin/bash

FORCE=false

while getopts "hfl" opt; do
	case $opt in
		h)
			echo "Usage: $0 [-hfl]"
			echo "  -h  Show this help message"
			echo "  -f  Force build all targets"
			exit 0
			;;
		f)
			FORCE=true
			;;
		*)
			echo "Invalid option: -$OPTARG" >&2
			exit 1
			;;
	esac
done

if [ ! -z "$GITHUB_STEP_SUMMARY" ] ; then
	echo "### Variant build results:" >> "$GITHUB_STEP_SUMMARY"
fi

extra/get_board_details.sh | jq -cr 'sort_by(.variant) | .[]' | while read -r item; do
	board=$(jq -cr '.board' <<< "$item")
	variant=$(jq -cr '.variant' <<< "$item")
	target=$(jq -cr '.target' <<< "$item")
	args=$(jq -cr '.args // ""' <<< "$item")

	if [ -z "$GITHUB_STEP_SUMMARY" ] ; then
		echo && echo
		echo "${board} (${variant})"
		echo "${board} (${variant})" | sed -e 's/./=/g'
	else
		echo "::group::=== ${board} (${variant}) ==="
	fi

	./extra/build.sh "$target" $args
	result=$?

	if [ -z "$GITHUB_STEP_SUMMARY" ] ; then
		echo
		echo "${variant} result: $result"
	else
		echo "::endgroup::"
		if [ $result -eq 0 ] ; then
			echo "- :white_check_mark: \`${variant}\`" >> "$GITHUB_STEP_SUMMARY"
		else
			echo "^^^^$(echo ${variant} | sed -e 's/./^/g')^^^^^  FAILED with $result!"
			echo "- :x: \`${variant}\`" >> "$GITHUB_STEP_SUMMARY"
		fi
	fi
	[ $result -ne 0 ] && ! $FORCE && exit $result
done

exit 0
