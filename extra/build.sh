#!/usr/bin/env bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -e

source venv/bin/activate

WEST_TOPDIR=$(west topdir)
ZEPHYR_BASE=${WEST_TOPDIR}/zephyr

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [ x$ZEPHYR_SDK_INSTALL_DIR == x"" ]; then
	SDK_PATH=$(west sdk list | grep path | tail -n 1 | cut -d ':' -f 2 | tr -d ' ')
	if [ x$SDK_PATH == x ]; then
		echo "ZEPHYR_SDK_INSTALL_DIR not set and no SDK found"
		exit 1
	fi
	export ZEPHYR_SDK_INSTALL_DIR=${SDK_PATH}
fi

usage() {
	cat << EOF
Usage:
	$0 [--key <file>] [--debug] <arduino_board> [<cmake_args>]
Build the loader for the given board.

The board must be defined in 'boards.txt' (e.g. 'giga'): its Zephyr target and
arguments are taken from that definition.

Additional <cmake_args> are passed as-is to 'west build', after the board's
arguments and --.

Options, accepted anywhere on the command line:
	--key <file>  on boards using MCUboot, the key used to sign the images;
	              without it, the publicly known development key of MCUboot
	              is used
	--debug       build the loader with extra/debug.conf
	-h, --help    show this help

Available boards, as defined in 'boards.txt':

EOF
	extra/get_board_details.sh |
		jq -r 'sort_by(.variant) | .[] | "\t\(.board)\t\(.target) \(.args)"' |
		column -ts$'\t'
	echo
}

SIGNING_KEY_PATH=""
# parse the arguments: -h and --debug can be given anywhere; the first other
# argument is the board name, the following ones are west arguments
DEBUG=0
board_name=""
cmake_args=()
while [ $# -gt 0 ]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--key)
			if [ $# -lt 2 ]; then
				log_msg error "--key needs a file argument" >&2
				exit 1
			fi
			SIGNING_KEY_PATH="$2"
			shift
			;;
		--debug)
			DEBUG=1
			;;
		-*)
			if [ -z "$board_name" ]; then
				log_msg error "Unknown option '$1'" >&2
				usage >&2
				exit 1
			fi
			cmake_args+=("$1")
			;;
		*)
			if [ -z "$board_name" ]; then
				board_name=$1
			else
				cmake_args+=("$1")
			fi
			;;
	esac
	shift
done

if [ -z "$board_name" ]; then
	usage >&2
	exit 1
fi

# the board details (target and arguments) come from boards.txt
chosen_board=$(extra/get_board_details.sh | jq -cr ".[] | select(.board == \"$board_name\") // empty")
if [ -z "$chosen_board" ]; then
	log_msg error "Board '$board_name' not defined in 'boards.txt'" >&2
	exit 1
fi
board=$(jq -cr '.board' <<< "$chosen_board")
target=$(jq -cr '.target' <<< "$chosen_board")
# the args field is a single string; split it on unquoted whitespace,
# keeping quotes so a quoted value with spaces stays one array element
arg_token='(?:[^\s"'\'']+|"[^"]*"|'\''[^'\'']*'\'')+'
mapfile -t args < <(jq -cr '.args' <<< "$chosen_board" | grep -oP "$arg_token")
# the cmake arguments come after the board's ones
args+=("${cmake_args[@]}")
upload_offset=$(jq -cr '.upload_offset' <<< "$chosen_board")

# extra CMake arguments for the loader build only
debug_args=()
if [ $DEBUG -eq 1 ]; then
	debug_args=(-DEXTRA_CONF_FILE=../extra/debug.conf)
fi

# Save the build version to loader/VERSION and for later use in local files;
# the burner image gets the same version
build_version=$(extra/get_core_version.sh loader/VERSION)
cp loader/VERSION burner/VERSION

# CMake arguments go after a '--' separator, which args may already contain
[[ " ${args[*]} " == *" -- "* ]] || args+=(--)
args+=(-DAPP_BUILD_VERSION=${build_version})

echo
echo "Build version: $build_version"
echo "Build target: $target ${args[*]} ${debug_args[*]}"

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

# The loader is built through sysbuild, which lets per-image sysbuild.cmake
# files add further images alongside it; a variant can configure sysbuild
# with a sysbuild.conf file.
sysbuild_args=()
KEY_FILE_NAME=""
if [ -f ${VARIANT_DIR}/sysbuild.conf ]; then
	sysbuild_args=(-DSB_CONF_FILE=$(realpath ${VARIANT_DIR}/sysbuild.conf))

	if grep -qs '^SB_CONFIG_BOOTLOADER_MCUBOOT=y' ${VARIANT_DIR}/sysbuild.conf && \
	   ! grep -qs '^SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE=y' ${VARIANT_DIR}/sysbuild.conf; then
		# MCUboot only boots signed images: sign them with the given
		# key, a variant default or the publicly known MCUboot dev key.
		# The key is copied to firmwares/<variant>/, where it can be used
		# to sign the sketches too.
		if [ -n "${SIGNING_KEY_PATH}" ]; then
			# directly provided key
			KEY_FILE_NAME=$(basename "${SIGNING_KEY_PATH}")
		elif key_files=(${VARIANT_DIR}/key/*.pem) && [ -f "${key_files[0]}" ]; then
			# variant key directory present
			if [ ${#key_files[@]} -gt 1 ]; then
				log_msg error "Multiple keys in ${VARIANT_DIR}/key/, use --key" >&2
				exit 1
			fi
			SIGNING_KEY_PATH=${key_files[0]}
			KEY_FILE_NAME=$(basename "${SIGNING_KEY_PATH}")
		else
			# fallback
			log_msg warning "No signing key given or found, using the insecure MCUboot development key"
			SIGNING_KEY_PATH=${WEST_TOPDIR}/bootloader/mcuboot/root-ec-p256.pem
			KEY_FILE_NAME=development-key.pem
		fi
		if [ ! -f "${SIGNING_KEY_PATH}" ]; then
			log_msg error "Signing key '${SIGNING_KEY_PATH}' not found" >&2
			exit 1
		fi
		echo "Signing key: ${SIGNING_KEY_PATH}"
		mkdir -p firmwares/${variant}
		rm -f firmwares/${variant}/*.pem
		cp "${SIGNING_KEY_PATH}" firmwares/${variant}/${KEY_FILE_NAME}
		# a Kconfig string: the value given with -D must include the quotes
		sysbuild_args+=(-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$(realpath firmwares/${variant}/${KEY_FILE_NAME})\")
	elif [ -n "${SIGNING_KEY_PATH}" ]; then
		log_msg warning "'${variant}' does not use MCUboot signing, --key ignored"
	fi
fi

# Run the main build process
west build -d ${BUILD_DIR} -b ${target} --sysbuild loader "${args[@]}" "${sysbuild_args[@]}" "${debug_args[@]}"

# The images built by sysbuild, as "<name> <build_dir>" lines from domains.yaml,
# the default (loader) image first. LOADER_DIR is the loader's own build tree.
domains=()
while IFS= read -r domain; do
	domains+=("$domain")
done < <(python3 -c '
import sys, yaml
d = yaml.safe_load(open(sys.argv[1]))
for x in sorted(d["domains"], key=lambda x: x["name"] != d["default"]):
    print(x["name"], x["build_dir"])
' ${BUILD_DIR}/domains.yaml)
LOADER_DIR=${domains[0]#* }

# The llext-edk target is not part of the sysbuild 'all', so build it separately
cmake --build ${LOADER_DIR} --target llext-edk

# Extract the generated EDK tarball and copy it to the variant directory
mkdir -p ${VARIANT_DIR} firmwares/${variant}
(set -e ; cd ${LOADER_DIR} && rm -rf llext-edk && tar xf zephyr/llext-edk.tar.Z)
rsync -a --delete ${LOADER_DIR}/llext-edk ${VARIANT_DIR}/
if [[ $variant == native_sim* ]] ; then
	# no toolchain picolibc for the host gcc: static sketches link the one
	# built for the loader, as ARM sketches link the toolchain one
	cp ${LOADER_DIR}/modules/picolibc/libc.a ${VARIANT_DIR}/libc.a
	# no picolibc-based libstdc++ for the host gcc either: use the one from
	# the Zephyr SDK toolchain for the host architecture (also installed by
	# bootstrap.sh)
	host_tc=$(uname -m)-zephyr-elf
	host_tc_dir=${ZEPHYR_SDK_INSTALL_DIR}/gnu/${host_tc}/${host_tc}
	cxx_inc=$(ls -d ${host_tc_dir}/include/c++/*/ 2>/dev/null | sort -V | tail -n 1)
	if [ -z "$cxx_inc" ] || [ ! -f ${host_tc_dir}/lib/libstdc++.a ] ; then
		echo "No libstdc++ found in ${host_tc_dir}"
		echo "Re-run extra/bootstrap.sh to install the ${host_tc} SDK toolchain"
		exit 1
	fi
	cp ${host_tc_dir}/lib/libstdc++.a ${host_tc_dir}/lib/libsupc++.a ${VARIANT_DIR}/
	rm -rf ${VARIANT_DIR}/libstdc++
	mkdir -p ${VARIANT_DIR}/libstdc++
	cp -r ${cxx_inc} ${VARIANT_DIR}/libstdc++/include
	# target specific headers in a fixed path, for boards.txt
	mv ${VARIANT_DIR}/libstdc++/include/${host_tc} ${VARIANT_DIR}/libstdc++/target
fi

# remove all inline comments in macro definitions
# (especially from devicetree_generated.h and sys/util_internal.h)
line_preproc_ok='^\s*#\s*(if|else|elif|endif)' # match conditional preproc lines
line_comment_only='^\s*\/\*' # match lines starting with comment
line_continuation='\\$' # match lines ending with '\'
c_comment='\s*\/\*.*?\*\/' # match C-style comments and any preceding space
perl -i -pe "s/${c_comment}//gs unless /${line_preproc_ok}/ || (/${line_comment_only}/ && !/${line_continuation}/)" $(find ${VARIANT_DIR}/llext-edk/include/ -type f)

# Make k_sleep_ticks()/k_busy_wait() external gnu_inline in the exported EDK headers
edk_include=${VARIANT_DIR}/llext-edk/include/zephyr/include
syscalls_hdrs="${edk_include}/generated/zephyr/syscalls/kernel.h ${edk_include}/generated/zephyr/syscalls/sleep.h"
api_hdrs="${edk_include}/zephyr/kernel.h ${edk_include}/zephyr/sleep.h"
edk_qual='extern inline __attribute__((always_inline, gnu_inline))'
edk_funcs='k_ticks_t k_sleep_ticks\(k_timeout_t timeout\)|void k_busy_wait\(uint32_t usec_to_wait\)'
perl -0pi -e "s/static inline ($edk_funcs)/$edk_qual \$1/g" $syscalls_hdrs
perl -0pi -e "s/__syscall ($edk_funcs);/$edk_qual \$1;/g" $api_hdrs

# Copy the outputs of each image as firmwares/<variant>/<image>.<ext>
for domain in "${domains[@]}"; do
	name=${domain%% *}
	image_dir=${domain#* }/zephyr
	rm -f firmwares/$variant/$name.*
	for ext in elf bin hex uf2 exe; do
		for kind in "" .signed; do
			if [ -f "$image_dir/zephyr$kind.$ext" ]; then
				cp "$image_dir/zephyr$kind.$ext" firmwares/$variant/$name$kind.$ext
			fi
		done
	done
	cp "$image_dir/zephyr.dts" firmwares/$variant/$name.dts
	cp "$image_dir/.config" firmwares/$variant/$name.config
done

# Boards with a separate bootloader also build the bootloader updater as the
# burner sysbuild image (see loader/sysbuild.cmake), signed under MCUboot
if [ -f firmwares/${variant}/mcuboot.hex ] && [ ! -f firmwares/${variant}/burner.signed.bin ]; then
	log_msg error "Signed bootloader updater for '${variant}' not built" >&2
	exit 1
fi
if [ -f firmwares/${variant}/bootloader.hex ] && [ ! -f firmwares/${variant}/burner.bin ]; then
	log_msg error "Bootloader updater for '${variant}' not built" >&2
	exit 1
fi

# Generate the provides.ld file for linked builds
echo "Generating exported symbol scripts"
if [ -f "${LOADER_DIR}/zephyr/zephyr.exe" ] ; then
	# native_sim: zephyr.elf is a partially linked object, final addresses
	# are only known in the host executable
	LOADER_ELF="${LOADER_DIR}/zephyr/zephyr.exe"
else
	LOADER_ELF="${LOADER_DIR}/zephyr/zephyr.elf"
fi
extra/gen_provides.py "${LOADER_ELF}" -T > ${VARIANT_DIR}/tls-syms.S
extra/gen_provides.py "${LOADER_ELF}" -L > ${VARIANT_DIR}/syms-dynamic.ld
extra/gen_provides.py "${LOADER_ELF}" -LF \
	"+kheap_llext_heap" \
	"+kheap__system_heap" \
	"*sketch_base_addr=_sketch_start" \
	"*sketch_max_size=_sketch_max_size" \
	"*loader_base_addr=_loader_start" \
	"*loader_max_size=_loader_max_size" \
	"malloc=__wrap_malloc" \
	"free=__wrap_free" \
	"realloc=__wrap_realloc" \
	"calloc=__wrap_calloc" \
	"random=__wrap_random" > ${VARIANT_DIR}/syms-static.ld

cmake -P extra/gen_arduino_files.cmake $variant

get_value_from_text_file() {
	local file=$1
	local field=$2

	grep -E "\<${field//./\\./}\>\s*=" $file | tail -n 1 | cut -d '=' -f 2- | tr -d '); '
}

get_board_field() {
	local field=$1
	local full_field_name="${board}.${field}"
	local match_regexp="${full_field_name//./\\.}" # escape dots

	# strip comments, then print the value of the field
	sed -e 's/\s*#.*//' boards.txt | sed -n -e "s/^${match_regexp}=//p"
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

	# update the actual field line (the value escaped for the sed replacement)
	if grep -qE "^${match_regexp}=" $file; then
		local escaped_value=$(sed -e 's/[\/&]/\\&/g' <<< "$value") # escape for sed
		sed -i -e "s/^${match_regexp}=.*/${full_field_name}=${escaped_value}/" $file
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

	# loader load address: start of loader partition, hex (exact), offset as above
	LOADER_START=$(get_value_from_text_file variants/${variant}/syms-static.ld '_loader_start')
	if [ -z "$upload_offset" ] ; then
		LOADER_ADDR=$LOADER_START
		LOADER_ADDR_COMMENT=""
	else
		LOADER_ADDR=$(printf "0x%X" $((LOADER_START - upload_offset)))
		LOADER_ADDR_COMMENT="$LOADER_START was offset by $upload_offset from ${board}.upload.offset"
	fi
	update_local_field "loader.address" "$LOADER_ADDR" "$LOADER_ADDR_COMMENT"

	LOADER_MAX_SIZE=$(get_value_from_text_file variants/${variant}/syms-static.ld '_loader_max_size')
	update_local_field "loader.maximum_size" $LOADER_MAX_SIZE

	# For "Burn Bootloader" on boards with a bootloader image (MCUboot built
	# by sysbuild, else the prebuilt bootloader.hex):
	# - with a hardware programmer, write the image with the board's
	#   swd_flash.tool;
	# - with the burner app, upload the burner image with the
	#   board's usual upload tool (copied as bootloader.tool.default).
	for image in mcuboot.hex bootloader.hex; do
		if [ -f firmwares/${variant}/$image ]; then
			update_local_field "swd_flash.image" "{runtime.platform.path}/firmwares/{build.variant}/$image"
			update_local_field "swd_flash.pattern" "{tools.patterns.swd_flash}"
			BOOTLOADER_TOOL=$(get_board_field "upload.tool.default")
			update_local_field "bootloader.tool.default" "$BOOTLOADER_TOOL"
			update_local_field "iap_flash.pattern" "{tools.patterns.iap_flash}"
			break
		fi
	done

	# MCUboot only boots signed images: upload the signed loader and burner
	if [ -f firmwares/${variant}/mcuboot.hex ]; then
		update_local_field "loader.basename" "loader.signed"
		update_local_field "burner.basename" "burner.signed"
	fi

	# the key to sign the sketches with, under MCUboot
	if [ -n "${KEY_FILE_NAME}" ]; then
		update_local_field "build.signing_key" "{runtime.platform.path}/firmwares/{build.variant}/${KEY_FILE_NAME}"
	fi

	# maximum sketch size: size of sketch partition, decimal (exact limit)
	CODE_SIZE=$(( $(get_value_from_text_file variants/${variant}/syms-static.ld '_sketch_max_size') ))
	update_local_field "upload.maximum_size" $CODE_SIZE

	# maximum data size: configured LLEXT heap size, decimal (larger bound, real limit is smaller)
	DATA_SIZE=$(( 1024*$(get_value_from_text_file firmwares/${variant}/loader.config 'CONFIG_LLEXT_HEAP_SIZE') ))
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

	if [ "$board" == "native_sim" ] ; then
		# native_sim can pose as any other board, to build its sketches on
		# the host with the same board define: add a 'mimic' menu option
		# for each, with its name and its build.board, and the variant.h
		# of the board for the native_sim variant.h to include (the single
		# quotes keep the double quotes of the path in the define)
		for mimic in $(grep -oP '^\w+(?=\.build\.board=)' boards.txt | grep -vx native_sim) ; do
			update_local_field "menu.mimic.${mimic}" "$(sed -n -e "s/^${mimic}\.name=//p" boards.txt)"
			update_local_field "menu.mimic.${mimic}.build.board" "$(sed -n -e "s/^${mimic}\.build\.board=//p" boards.txt)"
			mimic_variant=$(sed -n -e "s/^${mimic}\.build\.variant=//p" boards.txt)
			update_local_field "menu.mimic.${mimic}.build.posed_variant_flags" \
				"'-DARDUINO_POSED_VARIANT_H=\"{runtime.platform.path}/variants/${mimic_variant}/variant.h\"'"
		done
	fi
fi
