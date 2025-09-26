# SPDX-License-Identifier: Apache-2.0

if (CONFIG_ARDUINO_API)
  string(REPLACE "__" "_" NORMALIZED_BOARD_TARGET "${NORMALIZED_BOARD_TARGET}")

  if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${BOARD})
    set(variant_dir variants/${BOARD})
  elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${NORMALIZED_BOARD_TARGET})
    set(variant_dir variants/${NORMALIZED_BOARD_TARGET})
  else()
    message(FATAL_ERROR "Variant dir not found: variants/${BOARD}, variants/${NORMALIZED_BOARD_TARGET}")
  endif()

  if (NOT CONFIG_LLEXT)
    add_subdirectory(cores)
    add_subdirectory(libraries)
    zephyr_include_directories(${variant_dir})
  endif()

  zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/)
  if(NOT DEFINED ARDUINO_BUILD_PATH)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/CanMsg.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/CanMsgRingbuffer.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/Common.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/IPAddress.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/Print.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/Stream.cpp)
    zephyr_sources(${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/api/String.cpp)
  endif()

endif()
