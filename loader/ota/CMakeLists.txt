# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(ota)

target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_sketch_check.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_llext_exports.c)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR})

target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_sketch.c)
