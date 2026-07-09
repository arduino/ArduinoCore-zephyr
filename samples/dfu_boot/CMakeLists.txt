# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Resolve NORMALIZED_BOARD_TARGET and append the board variant overlay.
# Use EXTRA_DTC_OVERLAY_FILE so the local app.overlay is still auto-loaded.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)
set(EXTRA_DTC_OVERLAY_FILE
	${CMAKE_CURRENT_LIST_DIR}/../../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(usb-dfu)

include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
