#!/bin/bash

set -e

if [ -z "$1" ]; then
	echo "Usage: $0 VERSION"
	exit 1
fi

if [ ! -f platform.txt ]; then
	echo "Launch this script from the root core folder as ./extra/package.sh VERSION"
	exit 2
fi

PACKAGE=ArduinoCore-zephyr
VERSION=$1

TEMP_LIST=$(mktemp)

# import a basic list of files and directories
cat extra/package.lst | sed -e 's/\s*#.*//' | grep -v '^\s*$' > ${TEMP_LIST}

# add the board-specific files
extra/get_board_details.sh | jq -cr '.[]' | while read -r item; do
	variant=$(jq -cr '.variant' <<< "$item")
	echo "variants/${variant}/" >> ${TEMP_LIST}
	ls firmwares/zephyr-${variant}.* >> ${TEMP_LIST}
done
cat ${TEMP_LIST}
tar -cjhf ${PACKAGE}-${VERSION}.tar.bz2 -T ${TEMP_LIST} --transform "s,^,${PACKAGE}/,"
rm -f ${TEMP_LIST}
