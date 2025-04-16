#!/bin/bash

set -e

if [ x$ZEPHYR_SDK_INSTALL_DIR == x"" ]; then
	SDK_PATH=$(west sdk list | grep path | tail -n 1 | cut -d ':' -f 2 | tr -d ' ')
	if [ x$SDK_PATH == x ]; then
		echo "ZEPHYR_SDK_INSTALL_DIR not set and no SDK found"
		exit 1
	fi
	echo "ZEPHYR_SDK_INSTALL_DIR not set, using $SDK_PATH"
	export ZEPHYR_SDK_INSTALL_DIR=${SDK_PATH}
fi

if [[ $# -eq 0 ]]; then
	first_board=$(extra/get_board_details.sh | jq -cr '.[0]')
	target=$(jq -cr '.target' <<< "$first_board")
	args=$(jq -cr '.args' <<< "$first_board")
else
	target=$1
	shift
	args="$*"
fi

source venv/bin/activate

ZEPHYR_BASE=$(west topdir)/zephyr

# Get the variant name (NORMALIZED_BOARD_TARGET in Zephyr)
variant=$(extra/get_variant_name.sh $target)

if [ -z "${variant}" ] ; then
	echo "Failed to get variant name from '$target'"
	exit 1
fi

# Build the loader
BUILD_DIR=build/${variant}
VARIANT_DIR=variants/${variant}
rm -rf ${BUILD_DIR}
west build -d ${BUILD_DIR} -b ${target} loader -t llext-edk ${args}

# Extract the generated EDK tarball and copy it to the variant directory
mkdir -p ${VARIANT_DIR} firmwares
(set -e ; cd ${BUILD_DIR} && rm -rf llext-edk && tar xf zephyr/llext-edk.tar.Z)
rsync -a --delete ${BUILD_DIR}/llext-edk ${VARIANT_DIR}/

# remove all inline comments in macro definitions
# (especially from devicetree_generated.h and sys/util_internal.h)
line_preproc_ok='^\s*#\s*(if|else|elif|endif)' # match conditional preproc lines
line_comment_only='^\s*\/\*' # match lines starting with comment
line_continuation='\\$' # match lines ending with '\'
c_comment='\s*\/\*.*?\*\/' # match C-style comments and any preceding space
perl -i -pe "s/${c_comment}//gs unless /${line_preproc_ok}/ || (/${line_comment_only}/ && !/${line_continuation}/)" $(find ${VARIANT_DIR}/llext-edk/include/ -type f)
for ext in elf bin hex; do
    rm -f firmwares/zephyr-$variant.$ext
    if [ -f ${BUILD_DIR}/zephyr/zephyr.$ext ]; then
        cp ${BUILD_DIR}/zephyr/zephyr.$ext firmwares/zephyr-$variant.$ext
    fi
done

# Generate the provides.ld file for linked builds
echo "Exporting provides.ld"
READELF=${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-readelf
$READELF --wide -s ${BUILD_DIR}/zephyr/zephyr.elf  | c++filt  | grep FUNC | awk -F' ' '{print "PROVIDE("$8" = 0x"$2");"}' > ${VARIANT_DIR}/provides.ld
$READELF --wide -s ${BUILD_DIR}/zephyr/zephyr.elf  | c++filt  | grep kheap_llext_heap | awk -F' ' '{print "PROVIDE("$8" = 0x"$2");"}' >> ${VARIANT_DIR}/provides.ld
$READELF --wide -s ${BUILD_DIR}/zephyr/zephyr.elf  | c++filt  | grep kheap_llext_heap | awk -F' ' '{print "PROVIDE(kheap_llext_heap_size = "$3");"}' >> ${VARIANT_DIR}/provides.ld
$READELF --wide -s ${BUILD_DIR}/zephyr/zephyr.elf  | c++filt  | grep kheap__system_heap | awk -F' ' '{print "PROVIDE("$8" = 0x"$2");"}' >> ${VARIANT_DIR}/provides.ld
$READELF --wide -s ${BUILD_DIR}/zephyr/zephyr.elf  | c++filt  | grep kheap__system_heap | awk -F' ' '{print "PROVIDE(kheap__system_heap_size = "$3");"}' >> ${VARIANT_DIR}/provides.ld
cat ${BUILD_DIR}/zephyr/zephyr.map | grep __device_dts_ord | grep -v rodata | grep -v llext_const_symbol |  awk -F' ' '{print "PROVIDE("$2" = "$1");"}'  >> ${VARIANT_DIR}/provides.ld
TEXT_START=`cat variants/$variant/$variant.overlay | grep user_sketch: | cut -f2 -d"@" | cut -f1 -d"{"`
echo "PROVIDE(_sketch_start = 0x$TEXT_START);" >> ${VARIANT_DIR}/provides.ld

sed -i 's/PROVIDE(malloc =/PROVIDE(__wrap_malloc =/g' ${VARIANT_DIR}/provides.ld
sed -i 's/PROVIDE(free =/PROVIDE(__wrap_free =/g' ${VARIANT_DIR}/provides.ld
sed -i 's/PROVIDE(realloc =/PROVIDE(__wrap_realloc =/g' ${VARIANT_DIR}/provides.ld
sed -i 's/PROVIDE(calloc =/PROVIDE(__wrap_calloc =/g' ${VARIANT_DIR}/provides.ld
sed -i 's/PROVIDE(random =/PROVIDE(__wrap_random =/g' ${VARIANT_DIR}/provides.ld

cmake -P extra/gen_arduino_files.cmake $variant
