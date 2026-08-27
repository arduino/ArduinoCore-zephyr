# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Resolve NORMALIZED_BOARD_TARGET so the board variant overlay can be named.
#
# Both overlays are listed explicitly, variant first, so app.overlay is last and
# therefore wins: the variants set `chosen { zephyr,console = ... }` themselves
# (sercom6 on both assemblies) and this sample restates it rather than relying
# on that. Ordering here is plain list order -- deliberately not the
# DTC_OVERLAY_FILE-vs-EXTRA_DTC_OVERLAY_FILE concatenation detail, which is an
# undocumented Zephyr implementation detail that fails silently if it changes.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)
set(DTC_OVERLAY_FILE
	${CMAKE_CURRENT_LIST_DIR}/../../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay
	${CMAKE_CURRENT_LIST_DIR}/app.overlay
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(usb-dfu)

include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# <dfu_cookie.h> - the address/magic contract this sample shares with the
# loader and sketch. It lives beside the overlay that declares the reserved
# region so the two can never drift apart. The variants directory is not
# otherwise on the include path here: the core's top-level CMakeLists adds it,
# but only when CONFIG_ARDUINO_API is set, which this sample deliberately does
# not enable.
target_include_directories(app PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../../variants/arduino_nano_chandler_pic32ck1025gc01144
	${CMAKE_CURRENT_LIST_DIR}/../../loader/boards/arduino_nano_chandler_pic32ck1025gc01144/bfm
)

# The BFM writer and the Dual-Boot slot management moved to loader/boards/ so
# the loader can use them too - they are SoC behaviour, not sample behaviour.
# (They are NOT under variants/: arduino-cli scans that directory recursively
# when building a sketch, so anything there is compiled into every sketch.)
# They are added here rather than guarded internally, so "is this file in this
# image" is a build-system question.
target_sources_ifdef(CONFIG_APP_BFM_FLASH app PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../../loader/boards/arduino_nano_chandler_pic32ck1025gc01144/bfm/bfm_flash.c
	${CMAKE_CURRENT_LIST_DIR}/../../loader/boards/arduino_nano_chandler_pic32ck1025gc01144/bfm/bfm_boot.c
)
