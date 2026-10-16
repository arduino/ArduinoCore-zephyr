# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(hwinfo)

if (CONFIG_HWINFO_ARDUINO)
  target_sources_ifdef(CONFIG_SOC_SERIES_STM32H7X app PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/hwinfo_stm32h7x.c)

  target_sources_ifdef(CONFIG_BOARD_ARDUINO_PORTENTA_C33 app PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/hwinfo_portentac33.c)

  target_sources_ifdef(CONFIG_BOARD_ARDUINO_NANO_CONNECT app PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/hwinfo_nano_connect.c)
endif()
