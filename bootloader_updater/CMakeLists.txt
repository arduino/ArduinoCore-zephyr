# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# get value of NORMALIZED_BOARD_TARGET early
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)

# Same devicetree as the loader, so the updater is linked where the loader lives
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bootloader_updater LANGUAGES C)

# custom pinctrl state used by the variant overlays, see the core CMakeLists.txt
zephyr_compile_options(-DPINCTRL_STATE_ARDUINO=PINCTRL_STATE_PRIV_START)

if(NOT DEFINED BOOTLOADER_HEX)
	message(FATAL_ERROR "Set BOOTLOADER_HEX to the bootloader image to embed")
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
