#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Package the core into a distributable tar.zst archive.

set -e
RET=0

if [ -z "$1" ]; then
	echo "Usage: $0 ARTIFACT VERSION [OUTPUT_FILE]"
	exit 1
fi

if [ ! -f platform.txt ]; then
	echo "Launch this script from the root core folder."
	exit 2
fi

ARTIFACT=$1
VERSION=$2
OUTPUT_FILE=${3:-distrib/${ARTIFACT}-${VERSION}.tar.zst}

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

# we use variants for include because we filter on file paths
# and boards for exclude because we want to remove matching lines in boards.txt
BOARD_DETAILS=$(extra/get_board_details.sh)
if [ $ARTIFACT == "zephyr" ] ; then
	INCLUDED_VARIANTS=$(echo ${BOARD_DETAILS} | jq -cr ".[].variant")
	EXCLUDED_BOARDS=""
elif [ ! -f extra/artifacts/${ARTIFACT}.json ]; then
	echo "Unknown artifact '$ARTIFACT'."
	exit 3
else
	ARTIFACT_NAME="$(grep '"name"' extra/artifacts/${ARTIFACT}.json | head -n 1 | cut -d '"' -f 4) (${VERSION})"
	INCLUDED_VARIANTS=$(echo ${BOARD_DETAILS} | jq -cr "map(select(.artifact == \"$ARTIFACT\")) | .[].variant")
	EXCLUDED_BOARDS=$(echo ${BOARD_DETAILS} | jq -cr "map(select(.artifact != \"$ARTIFACT\")) | .[].board")
fi

log_msg group "Packaging ${ARTIFACT_NAME:-all variants} ($(basename $OUTPUT_FILE))"

# create a temporary boards.txt file with the correct list of boards
TEMP_BOARDS=$(mktemp -p . | sed 's/\.\///')
cat boards.txt boards.local.txt >> $TEMP_BOARDS
for board in $EXCLUDED_BOARDS ; do
	# remove (even commented) lines for excluded boards
	sed -i "/^\(\s*#\s*\)\?${board}\./d" $TEMP_BOARDS
done
# remove any per-board version lines
sed -i '/^\S*\.version=/d' $TEMP_BOARDS
# remove multiple empty lines
sed -i '/^$/N;/^\n$/D' $TEMP_BOARDS

# create a temporary platform.txt file with the correct version
TEMP_PLATFORM=$(mktemp -p . | sed 's/\.\///')
cat platform.txt > ${TEMP_PLATFORM}
[ -z "$ARTIFACT_NAME" ] || sed -ie "s/^name=.*/name=${ARTIFACT_NAME}/" ${TEMP_PLATFORM}
sed -i -e "s/^version=.*/version=$(extra/get_core_version.sh)/" ${TEMP_PLATFORM}

# create the list of files and directories to include
TEMP_INC=$(mktemp -p . | sed 's/\.\///')
echo ${TEMP_BOARDS} >> ${TEMP_INC}
echo ${TEMP_PLATFORM} >> ${TEMP_INC}
declutter_file extra/artifacts/_common.inc >> ${TEMP_INC}
declutter_file extra/artifacts/$ARTIFACT.inc >> ${TEMP_INC}
# $ARTIFACT.only should not be included to prevent duplicates with common
for variant in $INCLUDED_VARIANTS ; do
	echo "- ${variant}"
	echo "variants/${variant}/" >> ${TEMP_INC}
	# add the firmwares, if some are missing notify at end
	if ! ls -d firmwares/${variant} >> ${TEMP_INC} ; then
		log_msg error "No firmware for '${variant}' found."
		RET=3
	fi
	# boards with a separate bootloader also need the bootloader updater
	if [ -f firmwares/${variant}/bootloader.hex ] && [ ! -f firmwares/${variant}/burner.bin ] ; then
		log_msg error "No bootloader updater for '${variant}' found."
		RET=3
	fi
done

# deduplicate llext-edk headers shared by all included variants
# (CI only: this modifies the working tree)
if [ -n "$GITHUB_ACTIONS" ] ; then
	EDK_VARIANTS=$(for v in $INCLUDED_VARIANTS ; do
		if [ -d variants/$v/llext-edk/include ] ; then echo $v ; fi
	done)
	if [ $(echo $EDK_VARIANTS | wc -w) -ge 2 ] ; then
		extra/dedupe_includes.py --common-dir include $EDK_VARIANTS
	fi
fi
# shared headers, from the step above or from a synced deduplicated package
if [ -d variants/include ] ; then
	echo "variants/include/" >> ${TEMP_INC}
fi

# create the list of files and directories to exclude
TEMP_EXC=$(mktemp -p . | sed 's/\.\///')
artifact_exclusions $ARTIFACT >> ${TEMP_EXC}

mkdir -p $(dirname ${OUTPUT_FILE})
# --long=27: the variants share many files, far apart in the archive; a
# 128 MiB window is still extracted with no options by zstd and arduino-cli
tar -I "zstd -T0 -19 --long=27" -chf ${OUTPUT_FILE} -X ${TEMP_EXC} -T ${TEMP_INC} \
	--transform "s,${TEMP_BOARDS},boards.txt," \
	--transform "s,${TEMP_PLATFORM},platform.txt," \
	--transform "s,^,ArduinoCore-zephyr/,"
rm -f ${TEMP_INC} ${TEMP_EXC} ${TEMP_BOARDS} ${TEMP_PLATFORM}

log_msg endgroup

exit $RET
