#!/usr/bin/env bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Build the bootloader updater for an Arduino board. The bootloader image is
# taken from '<board>.bootloader.burn_image' and the result is saved to
# '<board>.bootloader.updater', as defined in 'boards.txt'.

set -e

source venv/bin/activate

if [ x$ZEPHYR_SDK_INSTALL_DIR == x"" ]; then
	SDK_PATH=$(west sdk list | grep path | tail -n 1 | cut -d ':' -f 2 | tr -d ' ')
	if [ x$SDK_PATH == x ]; then
		echo "ZEPHYR_SDK_INSTALL_DIR not set and no SDK found"
		exit 1
	fi
	export ZEPHYR_SDK_INSTALL_DIR=${SDK_PATH}
fi

if [ $# -ne 1 ] || [ x$1 == x"-h" ] || [ x$1 == x"--help" ]; then
	echo "Usage: $0 <arduino_board>"
	echo
	echo "Boards with a bootloader updater:"
	grep -oP '^\K[^.#]+(?=\.bootloader\.updater=)' boards.txt | sed 's/^/\t/'
	exit 0
fi

board=$1

get_board_path() {
	grep -E "^${board}\.$1=" boards.txt | cut -d '=' -f 2- | sed -e "s|{runtime.platform.path}|$PWD|"
}

chosen_board=$(extra/get_board_details.sh | jq -cr ".[] | select(.board == \"$board\") // empty")
bootloader_hex=$(get_board_path 'bootloader\.burn_image')
updater_bin=$(get_board_path 'bootloader\.updater')
if [ -z "$chosen_board" ] || [ -z "$bootloader_hex" ] || [ -z "$updater_bin" ]; then
	echo "Board '$board' has no bootloader updater defined in 'boards.txt'"
	exit 1
fi

target=$(jq -cr '.target' <<< "$chosen_board")
# split the args string on unquoted whitespace, as done in build.sh
arg_token='(?:[^\s"'\'']+|"[^"]*"|'\''[^'\'']*'\'')+'
mapfile -t args < <(jq -cr '.args' <<< "$chosen_board" | grep -oP "$arg_token")
variant=$(extra/get_variant_name.sh $target)

echo "Build target: $target ${args[*]}"
echo "Bootloader: $bootloader_hex"

BUILD_DIR=build/updater-${variant}
rm -rf ${BUILD_DIR}
west build -d ${BUILD_DIR} -b ${target} bootloader_updater "${args[@]}" -- -DBOOTLOADER_HEX="$bootloader_hex"

cp ${BUILD_DIR}/zephyr/zephyr.bin "$updater_bin"
echo "Updater saved to $updater_bin"
