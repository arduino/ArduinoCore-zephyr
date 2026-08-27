# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# get value of NORMALIZED_BOARD_TARGET early
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/${NORMALIZED_BOARD_TARGET}.overlay)
# Prepend: extra fragments (-DEXTRA_CONF_FILE) merge after the variant's conf.
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

# variant.c only (not .cpp): provides the board-specific _on_1200_bps().
set(variant_c ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}/variant.c)
if(EXISTS ${variant_c})
    target_sources(app PRIVATE ${variant_c})
endif()
# Variant headers (recovery_cookie.h, pfswap.h) shared with the bootloader.
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../variants/${NORMALIZED_BOARD_TARGET}
)

target_compile_definitions(app PUBLIC _XOPEN_SOURCE=700)
target_compile_definitions(app PUBLIC _GNU_SOURCE)

target_link_libraries(app PUBLIC blobs)
