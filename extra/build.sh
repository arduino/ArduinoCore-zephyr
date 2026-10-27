#!/usr/bin/env bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -e

source venv/bin/activate

ZEPHYR_BASE=$(west topdir)/zephyr

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [ x$ZEPHYR_SDK_INSTALL_DIR == x"" ]; then
	SDK_PATH=$(west sdk list | grep path | tail -n 1 | cut -d ':' -f 2 | tr -d ' ')
	if [ x$SDK_PATH == x ]; then
		echo "ZEPHYR_SDK_INSTALL_DIR not set and no SDK found"
		exit 1
	fi
	export ZEPHYR_SDK_INSTALL_DIR=${SDK_PATH}
fi

if [ $# -eq 0 ] || [ x$1 == x"-h" ] || [ x$1 == x"--help" ]; then
	cat << EOF
Usage:
	$0 <arduino_board>
	$0 <zephyr_board> [<west_args>]
Build the loader for the given target.

When given an <arduino_board> defined in 'boards.txt' (e.g. 'giga'), the actual
Zephyr board target and arguments are taken from that definition.

When given a <zephyr_board>, it is passed as the '-b' argument to 'west build'.
Additional <west_args> are passed as-is at the end of the command line.

Available targets, as defined in 'boards.txt':

EOF
	extra/get_board_details.sh |
		jq -r 'sort_by(.variant) | .[] | "\t\(.board)\t\(.target) \(.args)"' |
		column -ts$'\t'
	echo
	exit 0
fi

# try to find the board in boards.txt
chosen_board=$(extra/get_board_details.sh | jq -cr ".[] | select(.board == \"$1\") // empty")
if ! [ -z "$chosen_board" ]; then
	# found, use the target and args from there
	board=$(jq -cr '.board' <<< "$chosen_board")
	target=$(jq -cr '.target' <<< "$chosen_board")
	# the args field is a single string; split it on unquoted whitespace,
	# keeping quotes so a quoted value with spaces stays one array element
	arg_token='(?:[^\s"'\'']+|"[^"]*"|'\''[^'\'']*'\'')+'
	mapfile -t args < <(jq -cr '.args' <<< "$chosen_board" | grep -oP "$arg_token")
	upload_offset=$(jq -cr '.upload_offset' <<< "$chosen_board")

	# Check for debug flag and append
	if [ x$2 == x"--debug" ]; then
		args+=(-- -DEXTRA_CONF_FILE=../extra/debug.conf)
	fi
else
	# expect Zephyr-compatible target and args
	target=$1
	shift
	# keep each argument as a separate array element to preserve quoting
	args=("$@")
	chosen_board=$(extra/get_board_details.sh | jq -cr ".[] | select(.target == \"$target\") // empty")
	if [ ! -z "$chosen_board" ]; then
		board=$(jq -cr '.board' <<< "$chosen_board")
		upload_offset=$(jq -cr '.upload_offset' <<< "$chosen_board")
	else
		log_msg warning "No board for '$target' defined in 'boards.txt'. A proper definition is required to use the core."
	fi
fi

# Save the build version to loader/VERSION and for later use in local files
build_version=$(extra/get_core_version.sh loader/VERSION)

echo
echo "Build version: $build_version"
echo "Build target: $target ${args[*]}"

# Get the variant name (NORMALIZED_BOARD_TARGET in Zephyr)
variant=$(extra/get_variant_name.sh $target)

if [ -z "${variant}" ] ; then
	echo "Failed to get variant name from '$target'"
	exit 1
else
	echo "Build variant: $variant"
fi

# Build the loader
BUILD_DIR=build/${variant}
VARIANT_DIR=variants/${variant}
rm -rf ${BUILD_DIR}

# A variant shipping a sysbuild.conf boots through MCUboot built alongside the
# loader (its MCUboot config, mcuboot.conf/.overlay, is picked up by
# loader/sysbuild.cmake). Under sysbuild the loader's tree is
# ${BUILD_DIR}/loader and the llext-edk target is not part of 'all', so it is
# built separately.
if [ -f ${VARIANT_DIR}/sysbuild.conf ]; then
	IMAGE_DIR=${BUILD_DIR}/loader
	SB_CONF_FILE=$(realpath ${VARIANT_DIR}/sysbuild.conf) \
		west build -d ${BUILD_DIR} -b ${target} --sysbuild loader "${args[@]}"
	cmake --build ${IMAGE_DIR} --target llext-edk
else
	IMAGE_DIR=${BUILD_DIR}
	west build -d ${BUILD_DIR} -b ${target} loader -t llext-edk "${args[@]}"
fi

# Extract the generated EDK tarball and copy it to the variant directory
mkdir -p ${VARIANT_DIR} firmwares
(set -e ; cd ${IMAGE_DIR} && rm -rf llext-edk && tar xf zephyr/llext-edk.tar.Z)
rsync -a --delete ${IMAGE_DIR}/llext-edk ${VARIANT_DIR}/

# remove all inline comments in macro definitions
# (especially from devicetree_generated.h and sys/util_internal.h)
line_preproc_ok='^\s*#\s*(if|else|elif|endif)' # match conditional preproc lines
line_comment_only='^\s*\/\*' # match lines starting with comment
line_continuation='\\$' # match lines ending with '\'
c_comment='\s*\/\*.*?\*\/' # match C-style comments and any preceding space
perl -i -pe "s/${c_comment}//gs unless /${line_preproc_ok}/ || (/${line_comment_only}/ && !/${line_continuation}/)" $(find ${VARIANT_DIR}/llext-edk/include/ -type f)

# Make k_sleep()/k_busy_wait() external gnu_inline in the exported EDK headers
syscalls_hdr=${VARIANT_DIR}/llext-edk/include/zephyr/include/generated/zephyr/syscalls/kernel.h
kernel_hdr=${VARIANT_DIR}/llext-edk/include/zephyr/include/zephyr/kernel.h
edk_qual='extern inline __attribute__((always_inline, gnu_inline))'
edk_funcs='int32_t k_sleep\(k_timeout_t timeout\)|void k_busy_wait\(uint32_t usec_to_wait\)'
perl -0pi -e "s/static inline ($edk_funcs)/$edk_qual \$1/g" "$syscalls_hdr"
perl -0pi -e "s/__syscall ($edk_funcs);/$edk_qual \$1;/g" "$kernel_hdr"

for ext in elf bin hex uf2; do
    rm -f firmwares/zephyr-$variant.$ext
    if [ -f ${IMAGE_DIR}/zephyr/zephyr.$ext ]; then
        cp ${IMAGE_DIR}/zephyr/zephyr.$ext firmwares/zephyr-$variant.$ext
    fi
done
cp ${IMAGE_DIR}/zephyr/zephyr.dts firmwares/zephyr-$variant.dts
cp ${IMAGE_DIR}/zephyr/.config firmwares/zephyr-$variant.config

# The MCUboot companion image (sysbuild builds only)
rm -f firmwares/mcuboot-$variant.bin
if [ -f ${BUILD_DIR}/mcuboot/zephyr/zephyr.bin ]; then
    cp ${BUILD_DIR}/mcuboot/zephyr/zephyr.bin firmwares/mcuboot-$variant.bin
fi

SQUASH_ARGS=
if [ -n "$board" ] && [[ -f firmwares/zephyr-$variant.bin ]]; then
    # Dynamically extract squash arguments using the Python helper
    PLATFORM_PATH=$(pwd)
    BOOTLOADER_FILE="zephyr-${variant}.bin"
    
    SQUASH_ARGS=$(python3 extra/get_squash_args.py "boards.txt" "${board}" "${PLATFORM_PATH}" "${VARIANT_DIR}" "${BOOTLOADER_FILE}" "${variant}")
fi
# extract FLASH_ERASE_DIM as backup in case this information is not present 
# in the dts file 
if [[ ! -z $SQUASH_ARGS ]]; then
	if [[ "$SQUASH_ARGS" =~ --erase_flash_dim[[:space:]]+([^[:space:]]+) ]]; then
		FLASH_ERASE_DIM="${BASH_REMATCH[1]}"
	fi
fi

# Generate the provides.ld file for linked builds
echo "Generating exported symbol scripts"
extra/gen_provides.py "${IMAGE_DIR}/zephyr/zephyr.elf" -T > ${VARIANT_DIR}/tls-syms.S
extra/gen_provides.py "${IMAGE_DIR}/zephyr/zephyr.elf" -L > ${VARIANT_DIR}/syms-dynamic.ld
extra/gen_provides.py "${IMAGE_DIR}/zephyr/zephyr.elf" -LF \
	"+kheap_llext_heap" \
	"+kheap__system_heap" \
	"*sketch_base_addr=_sketch_start" \
	"*sketch_max_size=_sketch_max_size" \
	"*loader_max_size=_loader_max_size" \
	"malloc=__wrap_malloc" \
	"free=__wrap_free" \
	"realloc=__wrap_realloc" \
	"calloc=__wrap_calloc" \
	"random=__wrap_random" > ${VARIANT_DIR}/syms-static.ld

# Boards squashing the sketch after the loader in the MCUboot slot0 image:
# the sketch position depends on the loader size, so it is computed here.
if [ -n "$SQUASH_ARGS" ]; then
	echo "build.sh: (mcuboot) adding _sketch_start symbol to sym-static.ld"
	size_bytes=$(wc -c < "${IMAGE_DIR}/zephyr/zephyr.bin")
	
	# Read the array of values from the python script
	read -r -a SLOT0_INFO <<< $(python3 extra/extract_slot0_info.py "${IMAGE_DIR}/zephyr/zephyr.dts")
	
	if [ ${#SLOT0_INFO[@]} -eq 0 ]; then
		echo "Error: Could not find slot0_partition info in zephyr.dts"
		exit 1
	fi
	
	SLOT0_ADDR_HEX=${SLOT0_INFO[0]}
	SLOT0_DIM=${SLOT0_INFO[1]}
	DTS_FLASH_ERASE_DIM=${SLOT0_INFO[2]}
	
	calculated_start=$(( size_bytes + SLOT0_ADDR_HEX ))
	actual_start=$(printf "0x%X" "$calculated_start")
	echo "PROVIDE(_sketch_start = ${actual_start});" >> "${VARIANT_DIR}/syms-static.ld"
	
	echo "build.sh: (mcuboot) adding _sketch_max_size symbol to sym-static.ld"
	
	# Fallback to SQUASH_ARGS flash erase dimension if not found in DTS
	if [ "$DTS_FLASH_ERASE_DIM" != "None" ] && [ -n "$DTS_FLASH_ERASE_DIM" ]; then
		USED_FLASH_ERASE_DIM=$DTS_FLASH_ERASE_DIM
	else
		USED_FLASH_ERASE_DIM=$FLASH_ERASE_DIM
	fi
	
	if [ -z "$USED_FLASH_ERASE_DIM" ]; then
		echo "Error: Flash erase dimension not found in DTS or SQUASH_ARGS"
		exit 1
	fi
	
	# Calculate sketch_max_size = slot0_dimension - sketch_start_offset - (2 * erase_sector)
	calculated_max_size=$(( SLOT0_DIM - size_bytes - (2 * USED_FLASH_ERASE_DIM) ))
	actual_max_size=$(printf "0x%X" "$calculated_max_size")
	echo "PROVIDE(_sketch_max_size = ${actual_max_size});" >> "${VARIANT_DIR}/syms-static.ld"
fi

cmake -P extra/gen_arduino_files.cmake $variant

get_value_from_text_file() {
	local file=$1
	local field=$2

	grep -E "\<${field//./\\./}\>\s*=" $file | tail -n 1 | cut -d '=' -f 2- | tr -d '); '
}

update_local_field() {
	local field=$1
	local value="$2"
	local comment="$3"

	if [ -z "$board" ]; then
		local file="platform.local.txt"
		local full_field_name="${field}"
	else
		local file="boards.local.txt"
		local full_field_name="${board}.${field}"
	fi
	local match_regexp="${full_field_name//./\\.}" # escape dots

	if [ ! -f $file ] ; then
		cat << EOF > $file
#########################################################################################
#
# AUTO GENERATED FILE - DO NOT EDIT
# This file is manipulated by extra/build.sh; manual changes may be overwritten.
#
#########################################################################################

EOF
	fi

	if [ -n "$comment" ]; then
		# if there's a comment, add/update it as a commented-out line above the actual field
		if grep -qE "^# ${match_regexp}:" $file; then
			sed -i -e "s/^# ${match_regexp}:.*/# ${full_field_name}: ${comment}/" $file
		else
			echo "# ${full_field_name}: ${comment}" >> $file
		fi
	fi

	# update the actual field line
	if grep -qE "^${match_regexp}" $file; then
		sed -i -e "s/^${match_regexp}=.*/${full_field_name}=${value}/" $file
	else
		echo "${full_field_name}=${value}" >> $file
	fi
}

# update properties on boards.local.txt from the generated files
if [ ! -z "$board" ]; then

	# save version to both platform.local.txt and boards.local.txt:
	# - the platform one is reported by the IDE as _the_ core version;
	# - the board-specific one is used by the auto-update-loader feature
	#   (when developing, each board build should be tracked separately).
	board="" update_local_field "version" "$build_version"
	update_local_field "version" "$build_version"

	# sketch load address: start of sketch partition, hex (exact)
	CODE_ADDR=$(get_value_from_text_file variants/${variant}/syms-static.ld '_sketch_start')
	if [ -z "$upload_offset" ] ; then
		UPLOAD_ADDR=$CODE_ADDR
		UPLOAD_ADDR_COMMENT=""
	else
		UPLOAD_ADDR=$(printf "0x%X" $((CODE_ADDR - upload_offset)))
		UPLOAD_ADDR_COMMENT="$CODE_ADDR was offset by $upload_offset from ${board}.upload.offset"
	fi
	update_local_field "upload.address" "$UPLOAD_ADDR" "$UPLOAD_ADDR_COMMENT"

	LOADER_MAX_SIZE=$(get_value_from_text_file variants/${variant}/syms-static.ld '_loader_max_size')
	update_local_field "bootloader.maximum_size" $LOADER_MAX_SIZE

	# maximum sketch size: size of sketch partition, decimal (exact limit)
	CODE_SIZE=$(( $(get_value_from_text_file variants/${variant}/syms-static.ld '_sketch_max_size') ))
	update_local_field "upload.maximum_size" $CODE_SIZE

	# maximum data size: configured LLEXT heap size, decimal (larger bound, real limit is smaller)
	DATA_SIZE=$(( 1024*$(get_value_from_text_file firmwares/zephyr-${variant}.config 'CONFIG_LLEXT_HEAP_SIZE') ))
	update_local_field "upload.maximum_data_size" $DATA_SIZE

	# machine fields
	MACH_CPU=$(get_value_from_text_file variants/${variant}/machine_flags.txt 'mcpu')
	[ -z "$MACH_CPU" ] && MACH_CPU=$(get_value_from_text_file variants/${variant}/machine_flags.txt 'march')
	[ -z "$MACH_CPU" ] || update_local_field "build.architecture" $MACH_CPU
	[ -z "$MACH_CPU" ] || update_local_field "build.mcu" $MACH_CPU

	MACH_FPU=$(get_value_from_text_file variants/${variant}/machine_flags.txt 'mfpu')
	[ -z "$MACH_FPU" ] || update_local_field "build.fpu" $MACH_FPU

	MACH_FABI=$(get_value_from_text_file variants/${variant}/machine_flags.txt 'mfloat-abi')
	[ -z "$MACH_FABI" ] || update_local_field "build.float-abi" $MACH_FABI
fi
