#!/bin/bash

get_boards() {
	cat boards.txt | sed -e 's/#.*//' | grep -E '^.*\.build\.variant=' | sed -e 's/\.build\.variant=.*//'
}

get_board_field() {
	board=$1
	field=$2
	cat boards.txt | sed -e 's/#.*//' | grep -E "^$board\\.build\\.$field=" | cut -d '=' -f2- | sed -e 's/"/\"/g'
}

for BOARD in $(get_boards); do
	VARIANT=$(get_board_field $BOARD "variant")
	TARGET=$(get_board_field $BOARD "zephyr_target")
	ARGS=$(get_board_field $BOARD "zephyr_args")

	if [ -z "$TARGET" ] ; then
		echo "error: missing '$BOARD.build.zephyr_target'" 1>&2
		exit 1
	fi

	echo "{ \"board\": \"$BOARD\", \"variant\": \"$VARIANT\", \"target\": \"$TARGET\", \"args\": \"$ARGS\" }"
done | jq -crs .
