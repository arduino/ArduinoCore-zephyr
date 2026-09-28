# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(ota)

target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_sketch_check.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_llext_exports.c)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR})

target_sources_ifdef(CONFIG_BOARD_ARDUINO_PORTENTA_C33 app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ota_portentac33.c)

target_sources_ifdef(CONFIG_SOC_SERIES_STM32H7X app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ota_arduino_stm32h7.c)

target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ota_sketch.c)
