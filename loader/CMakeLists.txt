# SPDX-License-Identifier: Apache-2.0

#set(DTC_OVERLAY_FILE ${BOARD}.overlay)

cmake_minimum_required(VERSION 3.20.0)

# get value of NORMALIZED_BOARD_TARGET early
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay)
# Prepend rather than overwrite: a caller may pass extra fragments with
# -DEXTRA_CONF_FILE (boards.txt's build.zephyr_args, e.g. the Monica carrier's
# monica.conf, or extra/debug.conf). Those must be merged after the variant's
# own conf so they win.
set(EXTRA_CONF_FILE
    ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.conf
    ${EXTRA_CONF_FILE})

# Add dts/bindings to DTS_ROOT for custom bindings
list(APPEND DTS_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/blobs)

# Memory region relocation based on Kconfig
# Relocate LLEXT heap if region specified
if(NOT "${CONFIG_LLEXT_HEAP_REGION}" STREQUAL "")
    zephyr_code_relocate(FILES ${ZEPHYR_BASE}/subsys/llext/llext_kheap.c
                         LOCATION ${CONFIG_LLEXT_HEAP_REGION}_BSS_NOINIT)
endif()

# Relocate main stack if region specified
if(NOT "${CONFIG_MAIN_STACK_REGION}" STREQUAL "")
    zephyr_code_relocate(FILES ${ZEPHYR_BASE}/kernel/init.c
                         LOCATION ${CONFIG_MAIN_STACK_REGION}_BSS_NOINIT)
endif()


# for USB device stack NEXT
target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../cores/arduino/usb_device_descriptor.c
)

FILE(GLOB app_sources *.c)
target_sources(app PRIVATE ${app_sources})

# Build the variant's variant.c into the loader, so its board-specific
# _on_1200_bps() enters MCUboot serial recovery when the loader itself owns the
# USB CDC (no sketch running, or a sketch that never opened SerialUSB).
# variant.cpp (C++/Arduino initVariant) is NOT built into the loader - only the
# plain-C variant.c.
set(variant_c ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/variant.c)
if(EXISTS ${variant_c})
    target_sources(app PRIVATE ${variant_c})
endif()
# Board-specific loader code, if a board ships any: loader/boards/<board>/ is
# compiled into the loader only - unlike variant.c, which also goes into every
# sketch. Anything touching flash or the bootloader belongs here.
#
# 🔴 This must NOT live under variants/. arduino-cli scans the variant directory
# RECURSIVELY when it builds a sketch, so a subdirectory there is compiled into
# every sketch - which for this code means a sketch that fails to find
# <bfm_boot.h>, or worse, silently links a flash writer it has no business
# carrying. Found the hard way 2026-08-20.
set(board_loader_dir ${CMAKE_CURRENT_LIST_DIR}/boards/${NORMALIZED_BOARD_TARGET})
if(EXISTS ${board_loader_dir})
    FILE(GLOB board_loader_sources ${board_loader_dir}/*.c)
    # ...and whatever board machinery they build on. bfm/ is shared with
    # samples/dfu_boot rather than duplicated.
    FILE(GLOB board_bfm_sources ${board_loader_dir}/bfm/*.c)
    target_sources(app PRIVATE ${board_loader_sources} ${board_bfm_sources})
    target_include_directories(app PRIVATE
        ${board_loader_dir}
        ${board_loader_dir}/bfm
        # recovery_cookie.h and pfswap.h stay with the variant: the sketch and the
        # bootloader share them.
        ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}
    )
endif()

target_compile_definitions(app PUBLIC _XOPEN_SOURCE=700)
target_compile_definitions(app PUBLIC _GNU_SOURCE)

target_link_libraries(app PUBLIC blobs)
