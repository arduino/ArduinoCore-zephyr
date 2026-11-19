# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# get value of NORMALIZED_BOARD_TARGET early
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)

# Same devicetree as the loader, so the updater is linked where the loader lives
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Zephyr configures signing while find_package() processes its build system.
set(SIGNING_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/signing.cmake)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bootloader_updater LANGUAGES C)

# custom pinctrl state used by the variant overlays, see the core CMakeLists.txt
zephyr_compile_options(-DPINCTRL_STATE_ARDUINO=PINCTRL_STATE_PRIV_START)

set(BOOTLOADER_HEX ${CMAKE_CURRENT_LIST_DIR}/../bootloaders/${NORMALIZED_BOARD_TARGET}/bootloader.hex)
if(NOT EXISTS ${BOOTLOADER_HEX})
	message(FATAL_ERROR "No bootloader image for ${NORMALIZED_BOARD_TARGET}: ${BOOTLOADER_HEX}")
endif()

dt_nodelabel(boot_partition NODELABEL boot_partition REQUIRED)
dt_reg_size(boot_partition_size PATH ${boot_partition})

set(bootloader_bin ${ZEPHYR_BINARY_DIR}/bootloader.bin)
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

add_custom_command(
	OUTPUT ${bootloader_bin}
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/hex2bin.py
		${BOOTLOADER_HEX} ${bootloader_bin} ${boot_partition_size}
	DEPENDS ${BOOTLOADER_HEX} ${CMAKE_CURRENT_LIST_DIR}/scripts/hex2bin.py
)
generate_inc_file_for_target(app ${bootloader_bin} ${gen_dir}/bootloader.bin.inc)

target_sources(app PRIVATE src/main.c)

# Always export updater.bin, ready for the board's upload recipe.
set(updater_image ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.bin)
set(updater_padding 0)
if(CONFIG_BOOTLOADER_MCUBOOT)
	set(updater_image ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.signed.bin)
	if(CONFIG_MCUBOOT_BOOTLOADER_MODE_SWAP_USING_OFFSET)
		# MCUboot USB DFU writes from the start of slot1; the image must
		# begin at its second sector. The loader DFU also accepts this padding.
		dt_nodelabel(slot1_partition NODELABEL slot1_partition REQUIRED)
		get_filename_component(partitions ${slot1_partition} DIRECTORY)
		get_filename_component(slot1_flash ${partitions} DIRECTORY)
		dt_prop(updater_padding PATH ${slot1_flash} PROPERTY erase-block-size REQUIRED)
		dt_reg_size(slot1_size PATH ${slot1_partition})
		set(updater_size_args ${slot1_size})
	endif()
endif()

add_custom_command(
	OUTPUT ${ZEPHYR_BINARY_DIR}/updater.bin
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/prepare_updater.py
		${updater_image} ${ZEPHYR_BINARY_DIR}/updater.bin ${updater_padding} ${updater_size_args}
	DEPENDS zephyr_final ${updater_image} ${CMAKE_CURRENT_LIST_DIR}/scripts/prepare_updater.py
	VERBATIM
)
add_custom_target(updater ALL DEPENDS ${ZEPHYR_BINARY_DIR}/updater.bin)
