# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(ota)

target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_llext_exports.c)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR})
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../libraries/OTA/src)

if(CONFIG_OTA_BACKEND_FS)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ota_sketch_check.c
        ${CMAKE_CURRENT_LIST_DIR}/ota_sketch.c
        ${CMAKE_CURRENT_LIST_DIR}/ota_write_fs.c)

    target_sources_ifdef(CONFIG_SOC_SERIES_STM32H7X app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ota_arduino_stm32h7.c)
endif()

target_sources_ifdef(CONFIG_OTA_BACKEND_MCUBOOT app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ota_mcuboot.c)
