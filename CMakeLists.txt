# SPDX-License-Identifier: Apache-2.0

string(REPLACE "__" "_" NORMALIZED_BOARD_TARGET "${NORMALIZED_BOARD_TARGET}")

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${BOARD})
  set(variant_dir variants/${BOARD})
elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${NORMALIZED_BOARD_TARGET})
  set(variant_dir variants/${NORMALIZED_BOARD_TARGET})
else()
if (CONFIG_ARDUINO_API)
  message(FATAL_ERROR "Variant dir not found: variants/${BOARD}, variants/${NORMALIZED_BOARD_TARGET}")
endif()
endif()

if (CONFIG_ARDUINO_API AND NOT CONFIG_LLEXT_EDK)
add_subdirectory(cores)
add_subdirectory(libraries)
zephyr_include_directories(${variant_dir})
endif()

