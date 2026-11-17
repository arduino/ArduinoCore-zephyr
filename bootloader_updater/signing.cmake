# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Use the packaged Arduino signer instead of Zephyr's default imgtool script.
if(NOT CONFIG_BOOTLOADER_MCUBOOT)
	return()
endif()

find_program(ZEPHYR_SKETCH_TOOL NAMES zephyr-sketch-tool REQUIRED)
if(CONFIG_MCUBOOT_SIGNATURE_KEY_FILE STREQUAL "")
	message(FATAL_ERROR "The bootloader updater requires a signing key")
endif()

dt_nodelabel(slot0_partition NODELABEL slot0_partition REQUIRED)
dt_reg_size(signing_slot_size PATH ${slot0_partition})
if(NOT CONFIG_MCUBOOT_BOOTLOADER_MODE_SINGLE_APP)
	dt_nodelabel(signing_slot1 NODELABEL slot1_partition REQUIRED)
	dt_reg_size(signing_slot1_size PATH ${signing_slot1})
	if(signing_slot1_size LESS signing_slot_size)
		set(signing_slot_size ${signing_slot1_size})
	endif()
endif()

set(signing_output ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.signed.bin)
set_property(GLOBAL APPEND PROPERTY extra_post_build_commands COMMAND
	${ZEPHYR_SKETCH_TOOL} --sign
	--pem_file "${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE}"
	--image_version "${CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION}"
	--header_size ${CONFIG_ROM_START_OFFSET} --slot_size ${signing_slot_size}
	--output ${signing_output} ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.bin)
set_property(GLOBAL APPEND PROPERTY extra_post_build_byproducts ${signing_output})
set_property(TARGET zephyr_final APPEND PROPERTY LINK_DEPENDS
	"${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE}" ${ZEPHYR_SKETCH_TOOL})
set_target_properties(runners_yaml_props_target PROPERTIES bin_file ${signing_output})
